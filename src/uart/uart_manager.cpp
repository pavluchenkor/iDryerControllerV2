#include "uart_manager.h"
#include "ota_proxy_rp.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_UART
#define LOG_TAG "UART_MGR"
#include "debug_log.h"
// КРИТИЧНО: Устанавливаем уровень логов для этого файла

#include "controller/control.h"
#include "error/error_bus.h"
#include "error/error_table.h"
#include "hardware/hardware.h"
#include "leds/leds.h"
#include "menu/menu_bindings.h"
#include "menu/menu_eeprom.h"
#include "menu/menu_eeprom_io.h"
#include "menu/menu_types.h"
#include "version.h"
#include <ArduinoJson.h>
#include <hal/hal_arduino.h>
#include <pico/unique_id.h>

// HW-идентификатор контроллера. Задаётся при сборке через build_flags
// (-DHW_CTRL=\"rp2040-v1\"). Fallback — для сборок без флага. char[16] в Hello.
#ifndef HW_CTRL
#define HW_CTRL "rp2040-v1"
#endif

// =============================================================================
// РАСЧЕТ РАЗМЕРА JSON БУФЕРА ДЛЯ КОНФИГА
// =============================================================================
// Рассчитываем размер буфера на основе:
// - MENU__COUNT элементов меню (206 элементов)
// - MAX_UNITS максимальное количество юнитов (3)
// - Overhead ArduinoJson для internal representation
//
// Формула основана на эмпирических данных:
// - Фактический размер JSON ≈ 800-1000 байт
// - ArduinoJson требует в 3-4 раза больше памяти для internal structure
// - Добавляем запас для роста меню
#ifndef MAX_UNITS
#define MAX_UNITS 3
#endif

constexpr size_t ESTIMATED_PERUNIT_COUNT = MENU__COUNT * 35 / 100;                         // ~35% элементов per-unit
constexpr size_t CONFIG_JSON_SIZE = JSON_OBJECT_SIZE(3) +                                  // Корневой объект {"v", "full", "vals"}
                                    JSON_OBJECT_SIZE(MENU__COUNT) +                        // Объект vals с ~MENU__COUNT элементами
                                    JSON_ARRAY_SIZE(MAX_UNITS) * ESTIMATED_PERUNIT_COUNT + // Массивы для per-unit элементов
                                    MENU__COUNT * 12 +                                     // Строковые данные (id ключи + числа)
                                    768;                                                   // Запас на рост меню

// Проверка на этапе компиляции
static_assert(CONFIG_JSON_SIZE >= 2048, "CONFIG_JSON_SIZE должен быть >= 2048");
static_assert(CONFIG_JSON_SIZE <= 10240, "CONFIG_JSON_SIZE слишком большой (>10KB), проверьте расчет");

using namespace idryer::hal;

// Глобальный объект UartBridge
idryer::UartBridge uartBridge;

// =============================================================================
// ОЧЕРЕДЬ ОТЛОЖЕННЫХ КОМАНД
// =============================================================================

static PendingUartCommand g_uart_cmd_queue[UART_CMD_QUEUE_SIZE];
static volatile uint8_t g_uart_cmd_head = 0;
static volatile uint8_t g_uart_cmd_tail = 0;
static bool g_uart_link_ready = false;
static bool g_uart_link_info_available = false;
static uint32_t g_uart_link_ip = 0;
static char g_uart_link_ssid[33] = {0};
static bool g_claimed_blinked = false;
static uint8_t g_prev_cloud_state = 0; // предыдущее значение cloudState (LinkCloudState::Idle)

bool enqueueUartCommand(idryer::UartCmdCode code, uint8_t unitId, uint32_t arg0, uint32_t arg1) {
  uint8_t next_head = (g_uart_cmd_head + 1) % UART_CMD_QUEUE_SIZE;

  // Проверяем переполнение очереди
  if (next_head == g_uart_cmd_tail) {
    DEBUG_E("Command queue full, dropping command %d", (int)code);
    return false;
  }

  // Добавляем команду
  g_uart_cmd_queue[g_uart_cmd_head].code = code;
  g_uart_cmd_queue[g_uart_cmd_head].unitId = unitId;
  g_uart_cmd_queue[g_uart_cmd_head].arg0 = arg0;
  g_uart_cmd_queue[g_uart_cmd_head].arg1 = arg1;

  g_uart_cmd_head = next_head;

  DEBUG_I("Command %d enqueued for unit %d", (int)code, unitId);
  return true;
}

bool uartLinkReady() { return g_uart_link_ready; }
bool uartLinkInfoAvailable() { return g_uart_link_info_available; }
uint32_t uartLinkIp() { return g_uart_link_ip; }
const char *uartLinkSsid() { return g_uart_link_ssid; }

bool dequeueUartCommand(PendingUartCommand *out) {
  // Проверяем пустоту очереди
  if (g_uart_cmd_head == g_uart_cmd_tail) {
    return false;
  }

  // Извлекаем команду
  *out = g_uart_cmd_queue[g_uart_cmd_tail];
  g_uart_cmd_tail = (g_uart_cmd_tail + 1) % UART_CMD_QUEUE_SIZE;

  return true;
}

// Внешние зависимости из main.cpp
extern MenuState menu;
extern WorkTimeCounter wtc;
extern DryerController *controllers[NUM_UNITS];

extern HX711Multi *hx711MultiPtr;

static const MenuBinding *findBindingByPtr(const void *ptr) {
  if (!ptr) {
    return nullptr;
  }

  for (uint16_t i = 0; i < g_bindings_count; i++) {
    if (g_bindings[i].ptr == ptr) {
      return &g_bindings[i];
    }
  }

  return nullptr;
}

static bool isPerUnitMenuItem(const MenuItem &item) {
  if (item.type != MN_VALUE && item.type != MN_TOGGLE) {
    return false;
  }

  const MenuBinding *binding = findBindingByPtr(item.u.value.ptr);
  if (binding) {
    return binding->scope != SCOPE_GLOBAL;
  }

  // Fallback для пунктов без binding: опираемся на текущую структуру меню.
  if (item.parent == 2 || item.parent == 6) {
    return true;
  }
  if (item.parent >= 100 && item.parent < 142) {
    return true;
  }

  return false;
}

// =============================================================================
// CONFIG CHANGE HOOK
// =============================================================================

// Callback для отслеживания изменений конфига через menu hook
static void on_config_changed_hook(uint16_t itemId, uint8_t unit, const char *bind) {
  DEBUG_I("Config changed hook: itemId=%u unit=%u bind=%s", itemId, unit, bind);
  sendConfigDelta(itemId, unit);
}

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ
// =============================================================================

void initUartBridge(ISerial *serial, uint32_t baudRate) {
  g_uart_link_ready = false;
  g_uart_link_info_available = false;
  g_uart_link_ip = 0;
  g_uart_link_ssid[0] = '\0';
  g_claimed_blinked = false;
  g_prev_cloud_state = 0; // LinkCloudState::Idle
  uartBridge.begin(serial, baudRate);

  // Регистрация обработчиков
  uartBridge.setCommandHandler(handleUartCommand);
  uartBridge.setProfileHandler(handleUartProfileCommand);
  uartBridge.setHelloHandler(handleUartHello);
  uartBridge.setHeartbeatHandler(handleUartHeartbeat);
  uartBridge.setClaimStatusHandler(handleUartClaimStatus);
  uartBridge.setClaimCompleteHandler(handleUartClaimComplete);
  uartBridge.setConfigChunkHandler(handleUartConfigPush);
  uartBridge.setErrorHandler(handleUartError);

  uartBridge.setHelloAckHandler(handleUartHelloAck);

  // WebSocket Local Access обработчики
  uartBridge.setWsStatusHandler(handleUartWsStatus);

  // DRYER paired OTA — приёмник прошивки RP через UART-мост (Этап 3).
  uartBridge.setOtaAnnounceForMcuHandler(ota_rp::onAnnounce);
  uartBridge.setOtaChunkForMcuHandler(ota_rp::onChunkFragment);
  // Этап 5: периодический OtaStatus от ESP — обновляет partnerReady-state.
  uartBridge.setOtaStatusHandler([](const idryer::UartOtaStatusPayload &p, const idryer::UartFrameHeader &) { ota_rp::onPartnerStatus(p); });

  // Регистрируем hook для отслеживания изменений конфига
  menu_set_config_change_hook(on_config_changed_hook);

  DEBUG_I("UART Bridge initialized (Baud=%u)", baudRate);
}

// =============================================================================
// ФОРМИРОВАНИЕ И ОТПРАВКА HELLO
// =============================================================================

idryer::UartHelloPayload buildHelloPayload(const MenuState &menu, const WorkTimeCounter &wtc) {
  idryer::UartHelloPayload payload{};

  payload.role = idryer::UartRole::Rp2040Controller;
  payload.deviceType = idryer::UartDeviceType::Dryer;
  payload.firmwareVersion = (VERSION_MAJOR << 16) | (VERSION_MINOR << 8) | VERSION_PATCH;
  payload.workTimeCounter = wtc.getHours() * 3600 + wtc.getMinutes() * 60;
  strncpy(payload.hardwareVersion, HW_CTRL, sizeof(payload.hardwareVersion) - 1);
  pico_get_unique_board_id_string(payload.mcuSerial, sizeof(payload.mcuSerial));
  payload.unitsCount = menu.units_count;

  // Заполняем конфигурацию для каждого юнита
  for (uint8_t i = 0; i < 4; i++) {
    payload.units[i]._pad1 = 0;

    if (i < menu.units_count) {
      // Активный юнит
      payload.units[i].unitId = i;
      // Активный юнит - все capabilities включены
      payload.units[i].capabilities = static_cast<uint16_t>(idryer::UnitCaps::HEATER) | static_cast<uint16_t>(idryer::UnitCaps::FAN) | static_cast<uint16_t>(idryer::UnitCaps::SERVO) | static_cast<uint16_t>(idryer::UnitCaps::RH_AIR_SENSOR) | static_cast<uint16_t>(idryer::UnitCaps::TEMP_AIR_SENSOR) | static_cast<uint16_t>(idryer::UnitCaps::TEMP_HTR_SENSOR);

      // Весы: каждый юнит привязан к одному датчику HX711
      if (hx711MultiPtr && i < menu.scales_count) {
        payload.units[i].scales[0] = i; // U0->W0, U1->W1, U2->W2
        payload.units[i].scales[1] = 0xFF;
        payload.units[i].scales[2] = 0xFF;
        payload.units[i].scales[3] = 0xFF;
      } else {
        payload.units[i].scales[0] = 0xFF;
        payload.units[i].scales[1] = 0xFF;
        payload.units[i].scales[2] = 0xFF;
        payload.units[i].scales[3] = 0xFF;
      }

      // RFID: нет RFID ридеров
      payload.units[i].rfid[0] = 0xFF;
      payload.units[i].rfid[1] = 0xFF;
      payload.units[i].rfid[2] = 0xFF;
      payload.units[i].rfid[3] = 0xFF;
    } else {
      // Неактивный юнит - пустой слот
      payload.units[i].unitId = 0xFF;
      payload.units[i].capabilities = 0;
      payload.units[i].scales[0] = 0xFF;
      payload.units[i].scales[1] = 0xFF;
      payload.units[i].scales[2] = 0xFF;
      payload.units[i].scales[3] = 0xFF;
      payload.units[i].rfid[0] = 0xFF;
      payload.units[i].rfid[1] = 0xFF;
      payload.units[i].rfid[2] = 0xFF;
      payload.units[i].rfid[3] = 0xFF;
    }
  }

  return payload;
}

bool sendHello(const MenuState &menu, const WorkTimeCounter &wtc, bool ackRequired) {
  idryer::UartHelloPayload payload = buildHelloPayload(menu, wtc);
  bool success = uartBridge.sendHello(payload, ackRequired);
  DEBUG_W("Hello sent to ESP32: %s (units=%u, mcuSerial=%s)", success ? "OK" : "FAIL", menu.units_count, payload.mcuSerial);
  return success;
}

// =============================================================================
// ОТПРАВКА ДАННЫХ
// =============================================================================

void sendUartTelemetry() {
  // Serial.printf("[TX-TELE-PRE] menu.uc=%u (addr=%p)\n", menu.units_count, (void*)&menu.units_count);
  idryer::UartTelemetryPayload payload{};
  payload.count = menu.units_count;
  // Serial.printf("[TX-TELE-MID] payload.count=%u menu.uc=%u\n", payload.count, menu.units_count);

  for (uint8_t i = 0; i < menu.units_count; i++) {
    auto &entry = payload.units[i];
    DryerInputs inputs = controllers[i]->getInputs();

    entry.unitId = i;
    entry.setTemperature(inputs.airTempC); // NaN → sentinel (нет данных)
    entry.setHumidity(inputs.airHumRH);
    entry.heaterPowerPct = (uint8_t)(controllers[i]->heaterPower01() * 100);
    entry.fanOn = controllers[i]->fanOn() ? 1 : 0;
  }

  bool success = uartBridge.sendTelemetry(payload, true); // ACK required
  // [TEMP-DEBUG] прямой Serial.printf — DEBUG_I логгер на RP сейчас зажат.
  // Serial.printf("[TX-TELE] sent=%d count=%u u0_t10=%d u0_h10=%u\n",
  //               (int)success, payload.count,
  //               payload.units[0].temperatureC10, payload.units[0].humidityPct10);
  DEBUG_I("Telemetry sent: %s (units=%u)", success ? "OK" : "FAIL", payload.count);
}

void sendUartStatus() {
  idryer::UartStatusPayload payload{};
  payload.count = menu.units_count;
  payload.uptime = millis() / 1000;
  payload.ignoreExternalCmd = menu.ign_ext_cmd ? 1 : 0;
  const uint32_t nowMs = millis();

  for (uint8_t i = 0; i < menu.units_count; i++) {
    auto &entry = payload.units[i];
    entry.unitId = i;

    // Маппинг локального DryerMode в idryer::UartDryerMode
    DryerMode localMode = controllers[i]->mode();
    if (localMode == DryerMode::Idle) {
      entry.mode = idryer::UartDryerMode::Idle;
    } else if (localMode == DryerMode::Drying) {
      entry.mode = idryer::UartDryerMode::Drying;
    } else if (localMode == DryerMode::Storage) {
      entry.mode = idryer::UartDryerMode::Storage;
    } else if (localMode == DryerMode::Profile) {
      entry.mode = idryer::UartDryerMode::Profile;
    } else if (localMode == DryerMode::Error) {
      entry.mode = idryer::UartDryerMode::Fault;
    } else {
      entry.mode = idryer::UartDryerMode::Fault;
    }

    // sessionNum = сумма всех счетчиков сессий
    uint16_t dryingCount = 0, storageCount = 0, profileCount = 0;
    ee_read<uint16_t>(EE_OFF_DRYING_SESSION_COUNT, dryingCount);
    ee_read<uint16_t>(EE_OFF_STORAGE_SESSION_COUNT, storageCount);
    ee_read<uint16_t>(EE_OFF_PROFILE_SESSION_COUNT, profileCount);
    entry.sessionNum = dryingCount + storageCount + profileCount;
    entry.targetTempC10 = controllers[i]->getStatusTargetTempC10();
    entry.targetHumidityPct = controllers[i]->getStatusTargetHumidityPct();
    entry.durationMinutes = controllers[i]->getStatusDurationMinutes();
    entry.elapsedSeconds = controllers[i]->getStatusElapsedSeconds(nowMs);
    entry.totalRemainingSeconds = controllers[i]->getStatusTotalRemainingSeconds(nowMs);

    // Для профильного режима заполняем дополнительные поля
    if (localMode == DryerMode::Profile) {
      entry.currentStage = controllers[i]->getProfileCurrentStage();
      entry.totalStages = controllers[i]->getProfileTotalStages();
      entry.stageElapsedSeconds = controllers[i]->getProfileStageElapsedSeconds(nowMs);
      entry.stageRemainingSeconds = controllers[i]->getProfileStageRemainingSeconds(nowMs);
      entry.stagePhase = (controllers[i]->getProfilePhase() == ProfileState::Phase::Ramp) ? idryer::UartStagePhase::Ramp : idryer::UartStagePhase::Hold;
    } else {
      entry.currentStage = 0;
      entry.totalStages = 0;
      entry.stageElapsedSeconds = 0;
      entry.stageRemainingSeconds = 0;
      entry.stagePhase = idryer::UartStagePhase::Hold;
    }
  }

  bool success = uartBridge.sendStatus(payload);
  DEBUG_I("Status sent: %s (units=%u)", success ? "OK" : "FAIL", payload.count);
}

void sendUartHeartbeat() {
  idryer::UartHeartbeatPayload payload{};
  payload.uptimeSeconds = millis() / 1000;
  payload.wifiRssiDbm = 0;
  payload.errorsSinceBoot = 0;
  bool success = uartBridge.sendHeartbeat(payload);
  DEBUG_I("Heartbeat sent: %s (uptime=%lu)", success ? "OK" : "FAIL", payload.uptimeSeconds);
}

void sendUartWeights() {
  if (!hx711MultiPtr) return;

  idryer::UartWeightsPayload payload{};
  payload.count = 0;

  for (uint8_t i = 0; i < menu.scales_count && i < 4; i++) {
    float weight = hx711MultiPtr->getMassMulti(i);

    auto &entry = payload.weights[payload.count];
    entry.sensorId = i;
    // Multi-chamber: каждый датчик → свой юнит (W1→U1, W2→U2, ...)
    // Single-chamber (units_count==1): все датчики → U1, различаются по sensorId
    entry.unitId = (menu.units_count > 1) ? i : 0;
    entry.weightGramsC10 = (int16_t)(weight * 10);

    payload.count++;
  }

  if (payload.count > 0) {
    bool success = uartBridge.sendWeights(payload);
    DEBUG_I("Weights sent: %s (count=%u)", success ? "OK" : "FAIL", payload.count);
  }
}

bool sendUartClaimStart() {
  bool ok = uartBridge.sendClaimStart(true);
  DEBUG_I("ClaimStart sent: %s", ok ? "OK" : "FAIL");
  return ok;
}

bool sendRfidEvent(idryer::UartRfidEvent event, uint8_t readerId, const char *tag, uint8_t unitId) {
  idryer::UartRfidPayload payload{};
  payload.event = static_cast<uint8_t>(event);
  payload.readerId = readerId;
  payload.unitId = unitId;

  if (tag) {
    strncpy(payload.tag, tag, sizeof(payload.tag) - 1);
  } else {
    payload.tag[0] = '\0';
  }

  bool success = uartBridge.sendRfid(payload);
  DEBUG_I("RFID event sent: %s (event=%d, reader=%u, unit=%u, tag=%s)", success ? "OK" : "FAIL", (int)event, readerId, unitId, tag ? tag : "");

  return success;
}

// =============================================================================
// ОБРАБОТЧИКИ ВХОДЯЩИХ СООБЩЕНИЙ
// =============================================================================

void handleUartProfileCommand(const idryer::UartProfilePayload &payload, const idryer::UartFrameHeader &header) {
  DEBUG_I("Profile command: unitId=%d, stages=%d, startStage=%d", payload.unitId, payload.totalStages, payload.startStage);

  uint8_t unitId = payload.unitId;
  if (unitId >= NUM_UNITS || !controllers[unitId]) {
    DEBUG_W("Invalid unitId: %d", unitId);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::InvalidPayload);
    return;
  }

  if (payload.totalStages == 0 || payload.totalStages > 10) {
    DEBUG_W("Invalid totalStages: %d", payload.totalStages);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::InvalidPayload);
    return;
  }

  // Преобразуем UART ProfilePayload в локальный ProfileData
  ProfileData prof;
  prof.stageCount = payload.totalStages;

  for (uint8_t i = 0; i < payload.totalStages && i < 10; i++) {
    prof.stages[i].holdTempC = payload.stages[i].temp / 10; // температура × 10 → °C
    prof.stages[i].holdSeconds = payload.stages[i].hold;    // секунды
    prof.stages[i].rampSeconds = payload.stages[i].ramp;    // секунды
  }

  uint8_t startStage = (payload.startStage < payload.totalStages) ? payload.startStage : 0;

  // ✅ Инкремент счетчика перенесен в DryerController::startProfile()
  // Это гарантирует инкремент независимо от источника команды (меню, UART, и т.д.)

  controllers[unitId]->startProfile(prof, startStage);
  DEBUG_I("Profile started on U%u: %u stages from stage %u", unitId, prof.stageCount, startStage);

  uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::None);
}

void handleUartCommand(const idryer::UartCmdPayload &payload, const idryer::UartFrameHeader &header) {
  DEBUG_I("Command: code=%d, state=%d, unitId=%d", payload.command, payload.targetState, payload.unitId);

  // Gate: ignore_external_cmd (source-of-truth — menu/NVS, синхронизация на
  // ESP-стороне через UART Status). Блокирует команды-действия (Start, Stop,
  // WriteRfid, Find, ClearErrors). Whitelist read/diag: GetConfig, ReadRfid,
  // WifiStatus — всегда проходят. ResetFault блокируется (это активное
  // вмешательство в состояние контроллера).
  // Paired OTA Этап 5: ota_rp::isCommitInProgress() добавлен в gate — окно
  // между OtaCommitNow и rp2040.reboot() (~250 мс) команды дропаются, иначе
  // можно случайно запустить нагрев прямо перед swap.
  if (menu.ign_ext_cmd || ota_rp::isCommitInProgress()) {
    const bool isReadOnly = payload.command == idryer::UartCmdCode::GetConfig || payload.command == idryer::UartCmdCode::ReadRfid || payload.command == idryer::UartCmdCode::WifiStatus;
    if (!isReadOnly) {
      DEBUG_W("[GATE] rejected cmd=%d (ignore_external_cmd=true)", payload.command);
      uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::ExternalCmdIgnored);
      return;
    }
  }

  uint8_t unitId = payload.unitId;
  if (unitId >= NUM_UNITS || !controllers[unitId]) {
    DEBUG_W("Invalid unitId: %d", unitId);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::InvalidPayload);
    return;
  }

  switch (payload.command) {
  case idryer::UartCmdCode::Start: {
    // arg0 = температура*10, arg1 зависит от targetState
    float tempC = (payload.arg0 > 0) ? (payload.arg0 / 10.0f) : 55.0f;

    // Маппинг targetState в локальный DryerMode
    DryerMode targetMode;
    if (payload.targetState == (uint8_t)idryer::UartDryerMode::Drying) {
      // DRYING: arg1 = длительность в минутах
      targetMode = DryerMode::Drying;
      uint32_t duration = (payload.arg1 > 0) ? payload.arg1 : 240;
      DEBUG_I("Command START DRYING: unitId=%d, temp=%.1f°C, duration=%u min", unitId, tempC, duration);
      controllers[unitId]->requestMode(targetMode, tempC, duration);
    } else if (payload.targetState == (uint8_t)idryer::UartDryerMode::Storage) {
      // STORAGE: humidity / priority берём из меню (NVS) — то же, что делает
      // локальный UI в user_menu_callbacks.cpp:169-174. payload.arg1
      // намеренно игнорируется: канонический контракт (Variant B,
      // 2026-05-01) требует менять storage_hum / storage_hum_priority
      // через commands/set заранее, а не передавать в commands/storage.
      targetMode = DryerMode::Storage;
      uint8_t hum = (uint8_t)menu.storage_hum[unitId];
      bool byHum = menu.storage_hum_priority[unitId];
      DEBUG_I("Command START STORAGE: unitId=%d, temp=%.1f°C, hum=%u%%, byHum=%d", unitId, tempC, (unsigned)hum, (int)byHum);
      controllers[unitId]->requestMode(targetMode, tempC, /*minutes*/ 0, hum, byHum);
    } else {
      DEBUG_W("Unknown targetState: %d, defaulting to DRYING", payload.targetState);
      targetMode = DryerMode::Drying;
      controllers[unitId]->requestMode(targetMode, tempC, 240);
    }

    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::None);
    break;
  }

  case idryer::UartCmdCode::Stop:
    controllers[unitId]->stop();
    DEBUG_I("Unit %d stopped", unitId);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::None);
    break;

  case idryer::UartCmdCode::GetConfig:
    DEBUG_I("Command GET_CONFIG");
    sendFullConfig(menu);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::None);
    break;

  case idryer::UartCmdCode::WriteRfid: {
    // Sync-фикс: staging должен быть armed ДО того, как парсер UART разберёт
    // уже поступившие в буфер фрагменты. Enqueue в main loop создаёт окно,
    // в котором frag1 отбрасывается с "staging not armed". Армируем синхронно.
    extern bool rfidArmWriteStagingByUnit(uint8_t unitId, uint32_t expectedSize, uint32_t verifyCode);
    bool armed = rfidArmWriteStagingByUnit(unitId, payload.arg0, payload.arg1);
    DEBUG_I("[UART] WriteRfid sync-arm unit=%u size=%u verify=%u armed=%s", unitId, (unsigned)payload.arg0, (unsigned)payload.arg1, armed ? "OK" : "FAIL");
    uartBridge.sendCommandAck(header.sequence, armed ? idryer::UartErrCode::None : idryer::UartErrCode::InvalidPayload);
    break;
  }

  case idryer::UartCmdCode::Find:
  case idryer::UartCmdCode::ReadRfid:
  case idryer::UartCmdCode::ResetFault:
  case idryer::UartCmdCode::ClearErrors:
    // Команды обрабатываются в main loop
    enqueueUartCommand(payload.command, unitId, payload.arg0, payload.arg1);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::None);
    break;

  case idryer::UartCmdCode::WifiStatus:
    DEBUG_I("Command WIFI_STATUS - request IP address");
    // Отправим HelloAck с текущим WiFi статусом (если есть)
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::None);
    break;

  default:
    DEBUG_W("Unknown command: %d", payload.command);
    uartBridge.sendCommandAck(header.sequence, idryer::UartErrCode::InvalidPayload);
    break;
  }
}

void handleUartHello(const idryer::UartHelloPayload &payload, const idryer::UartFrameHeader &header) {
  DEBUG_W("ESP32 Hello Request received");
  g_uart_link_ready = true;

  // Проверка MAJOR версии протокола
  uint8_t link_major = (payload.firmwareVersion >> 16) & 0xFF;
  uint8_t mcu_major = VERSION_MAJOR;

  if (link_major != mcu_major) {
    DEBUG_C("Protocol version mismatch: MCU=%u, LINK=%u", mcu_major, link_major);

    // Paired OTA Этап 4 — self-healing перед CRITICAL.
    // (1) Есть верифицированная прошивка с подходящим major в LittleFS?
    //     → ставим флаг swap-on-idle для Этапа 5, CRITICAL не постим.
    if (ota_rp::hasVerifiedImageForMajor(link_major)) {
      ota_rp::scheduleSwapOnIdle();
      DEBUG_I("self-healing: verified image for major=%u → swap scheduled on idle", link_major);
      sendHello(menu, wtc, false);
      return;
    }
    // (2) Уже в работе запрос OTA через ESP (OtaCheckRequest отправлен, ждём
    //     OtaAnnounceForMcu или TTL)? → не спамим, ждём.
    if (ota_rp::isCheckRequestPending(millis())) {
      DEBUG_D("self-healing: OtaCheckRequest pending → wait, no CRITICAL");
      sendHello(menu, wtc, false);
      return;
    }
    // (3) Шлём запрос на публикацию check_update от имени RP. ESP получит
    //     OtaCheckRequest и опубликует MQTT events/firmware_check_update с
    //     controllerType=RP2040. Backend либо запушит обновление (→
    //     OtaAnnounceForMcu → onAnnounce → сброс pending) либо нет (→ TTL 60c).
    idryer::UartOtaCheckRequestPayload req{};
    req.currentVersion = ((uint32_t)VERSION_MAJOR << 16) | ((uint32_t)VERSION_MINOR << 8) | (uint32_t)VERSION_PATCH;
    if (uartBridge.sendOtaCheckRequest(req)) {
      ota_rp::markCheckRequestSent(millis());
      DEBUG_I("self-healing: sent OtaCheckRequest v=%u.%u.%u → awaiting announce", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
    } else {
      // Не смогли отправить — молчим. ESP периодически (каждые 5с) шлёт Hello,
      // следующий handleUartHello попробует снова. Никаких CRITICAL: если UART
      // совсем сломан, Hello вообще не приходит и errorbus получит UART-ошибки
      // через другие пути.
      DEBUG_W("self-healing: sendOtaCheckRequest failed, will retry on next Hello");
    }
  } else {
    DEBUG_I("Protocol versions match: MCU=%u, LINK=%u", mcu_major, link_major);
  }

  // Отправляем полный Hello (bidirectional handshake)
  sendHello(menu, wtc, false);
  // sendWsEnable отправляется в handleUartHelloAck, когда ESP32 завершила обработку Hello
}

void handleUartHelloAck(const idryer::UartHelloAckPayload &payload, const idryer::UartFrameHeader &header) {
  g_uart_link_ip = payload.ipAddress;
  strncpy(g_uart_link_ssid, payload.ssid, sizeof(g_uart_link_ssid) - 1);
  g_uart_link_ssid[sizeof(g_uart_link_ssid) - 1] = '\0';
  g_uart_link_info_available = (g_uart_link_ip != 0) || (g_uart_link_ssid[0] != '\0');

  uint32_t ip = payload.ipAddress;
  DEBUG_W("ESP32 HelloAck: IP=%u.%u.%u.%u, SSID='%s'", (ip >> 0) & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, payload.ssid);

  sendWsEnable(menu.ws_enabled);
}

void handleUartHeartbeat(const idryer::UartHeartbeatPayload &payload, const idryer::UartFrameHeader &header) {
  g_uart_link_ready = true;

  // Отслеживаем переходы cloudState для LED индикации
  uint8_t newState = static_cast<uint8_t>(payload.cloudState);
  uint8_t oldState = g_prev_cloud_state;

  if (newState != oldState) {
    DEBUG_W("CloudState: %u -> %u", oldState, newState);

    // WiFi подключён (вышли из WifiConnecting)
    if (newState > (uint8_t)idryer::UartLinkCloudState::WifiConnecting && oldState <= (uint8_t)idryer::UartLinkCloudState::WifiConnecting) {
      ledsShowWebBreath(LedColors::GREEN, 1000, 3000);
      g_uart_link_info_available = true;
      DEBUG_W("WiFi connected — green blink");
    }

    // MQTT подключён (Online = 7)
    if (newState == (uint8_t)idryer::UartLinkCloudState::Online && oldState != (uint8_t)idryer::UartLinkCloudState::Online) {
      ledsShowWebBreath(LedColors::BLUE, 1000, 3000);
      DEBUG_W("MQTT online — blue blink");
    }

    g_prev_cloud_state = newState;
  }

  // RSSI для обратной совместимости
  if (payload.wifiRssiDbm < 0) {
    g_uart_link_info_available = true;
  }

  DEBUG_I("ESP32 Heartbeat: uptime=%lu, rssi=%d, errors=%u, cloud=%u", payload.uptimeSeconds, payload.wifiRssiDbm, payload.errorsSinceBoot, payload.cloudState);
}

void handleUartClaimStatus(const idryer::UartClaimStatusPayload &payload, const idryer::UartFrameHeader &header) {
  // Конвертируем Unix timestamp в читаемый формат
  uint32_t now = payload.expiresAt - payload.remainingSeconds;
  DEBUG_I("ESP32 ClaimStatus: pin='%s', expires=%lu (server_time=%lu, "
          "remaining=%lu)",
          payload.pin, payload.expiresAt, now, payload.remainingSeconds);
  if (payload.status == idryer::UartClaimStatus::Claimed) {
    if (!g_claimed_blinked) {
      ledsShowWebBreath(LedColors::BLUE, 1000, 5000); // 5 дыханий
      g_claimed_blinked = true;
    }
  } else if (payload.status == idryer::UartClaimStatus::Error) {
    // ESP32 detected RP2040 serial mismatch — show error indication on LED strip.
    ledsShowWebBreath(LedColors::RED, 300, 10000);
    g_claimed_blinked = false;
    DEBUG_E("ESP32 ClaimStatus: ERROR (mcuSerial mismatch — wrong RP2040 connected)");
  } else {
    g_claimed_blinked = false;
  }
  // claim_on_status(payload, millis()); // DISABLED: claiming not used
}

void handleUartClaimComplete(const idryer::UartClaimCompletePayload &payload, const idryer::UartFrameHeader &header) {
  DEBUG_I("ESP32 ClaimComplete: success=%u deviceId='%s'", payload.success, payload.deviceId);
  if (payload.success && !g_claimed_blinked) {
    ledsShowWebBreath(LedColors::BLUE, 1000, 5000); // 5 дыханий
    g_claimed_blinked = true;
  }
  // claim_on_complete(payload, millis()); // DISABLED: claiming not used
}

void handleUartError(const idryer::UartErrorPayload &payload, bool remote) {
  DEBUG_E("%s error: code=%d", remote ? "Remote" : "Local", payload.code);
  // CrcMismatch/InvalidPayload на kind=OtaChunkForMcu означает что фрагмент
  // битый. Если не сбросить frag-accumulator, следующий валидный FRAGMENT
  // допишется к битому буферу — header съедет, commandId/chunkIdx будут
  // мусорные. Делаем abort активного accumulator'а; ESP переселлёт chunk
  // целиком по timeout ack (retry-loop в pushChunkToRp).
  if (payload.code == idryer::UartErrCode::CrcMismatch || payload.code == idryer::UartErrCode::InvalidPayload) {
    ota_rp::onUartFrameDropped();
  }
}

// =============================================================================
// REMOTE CONFIG
// =============================================================================

// Глобальный счетчик ревизий конфига
static uint16_t g_config_revision = 1; // Начинаем с 1, инкрементируется при изменениях

bool sendFullConfig(const MenuState &menu) {
  DEBUG_E("\n[UART_TX] sendFullConfig() START: units=%u rev=%u\n", menu.units_count, g_config_revision);
  DEBUG_I("[UART_TX] JSON buffer: size=%u bytes (auto-calculated for MENU__COUNT=%u)", CONFIG_JSON_SIZE, MENU__COUNT);

  // Размер буфера рассчитывается автоматически на основе MENU__COUNT
  StaticJsonDocument<CONFIG_JSON_SIZE> doc;

  doc["v"] = g_config_revision;
  doc["full"] = true; // Маркер полного конфига (не DELTA)
  JsonObject vals = doc.createNestedObject("vals");

  // Проходим по всем элементам меню с значениями
  for (uint16_t i = 0; i < MENU__COUNT; i++) {
    const MenuItem &item = g_menu[i];

    // Пропускаем элементы без значений
    if (item.type != MN_VALUE && item.type != MN_TOGGLE) {
      continue;
    }

    const ValueSpec &spec = item.u.value;
    if (!spec.ptr) {
      continue;
    }

    char idStr[8];
    snprintf(idStr, sizeof(idStr), "%u", item.id);

    bool isPerUnit = isPerUnitMenuItem(item);

    if (isPerUnit) {
      // Per-unit настройка - массив значений
      JsonArray arr = vals.createNestedArray(idStr);
      for (uint8_t u = 0; u < menu.units_count; u++) {
        void *ptr = (uint8_t *)spec.ptr + u * item.ee_size;

        switch (spec.vtype) {
        case VT_F32:
          arr.add(*(float *)ptr);
          break;
        case VT_U32:
          arr.add(*(uint32_t *)ptr);
          break;
        case VT_I32:
          arr.add(*(int32_t *)ptr);
          break;
        case VT_U16:
          arr.add(*(uint16_t *)ptr);
          break;
        case VT_U8:
          arr.add(*(uint8_t *)ptr);
          break;
        case VT_BOOL:
          arr.add(*(bool *)ptr);
          break;
        }
      }
    } else {
      // Global настройка - одно значение
      switch (spec.vtype) {
      case VT_F32:
        vals[idStr] = *(float *)spec.ptr;
        break;
      case VT_U32:
        vals[idStr] = *(uint32_t *)spec.ptr;
        break;
      case VT_I32:
        vals[idStr] = *(int32_t *)spec.ptr;
        break;
      case VT_U16:
        vals[idStr] = *(uint16_t *)spec.ptr;
        break;
      case VT_U8:
        vals[idStr] = *(uint8_t *)spec.ptr;
        break;
      case VT_BOOL:
        vals[idStr] = *(bool *)spec.ptr;
        break;
      }
    }
  }

  // Проверка переполнения буфера
  if (doc.overflowed()) {
    DEBUG_E("[UART_TX] ERROR: JSON buffer overflow! Increase CONFIG_JSON_SIZE constant!");
    return false;
  }

  // Сериализуем в строку
  String jsonStr;
  serializeJson(doc, jsonStr);

  // Статистика использования памяти
  size_t memUsed = doc.memoryUsage();
  size_t memCapacity = doc.capacity();
  uint8_t memPercent = (memUsed * 100) / memCapacity;
  DEBUG_I("[UART_TX] JSON memory: used=%u/%u bytes (%u%%), serialized=%u bytes", memUsed, memCapacity, memPercent, jsonStr.length());

  Serial.printf("[UART_TX] CONFIG JSON (%u bytes):\n%s\n", jsonStr.length(), jsonStr.c_str());
  Serial.printf("[UART_TX] Sending %u bytes, rev=%u\n", jsonStr.length(), g_config_revision);

  // Отправляем через ConfigPushChunk с фрагментацией
  uint16_t totalSize = jsonStr.length();
  uint16_t transferId = millis() & 0xFFFF; // Используем timestamp как transferId
  uint16_t offset = 0;
  uint16_t chunkIndex = 0;

  while (offset < totalSize) {
    idryer::UartConfigChunkPayload payload{};

    // Заполняем заголовок
    payload.transferId = transferId;
    payload.totalSize = (chunkIndex == 0) ? totalSize : 0; // totalSize только в первом чанке
    payload.chunkIndex = chunkIndex;

    // Копируем данные
    uint16_t remaining = totalSize - offset;
    uint16_t dataLen = (remaining < (idryer::UART_MAX_PAYLOAD - idryer::UART_CONFIG_CHUNK_HEADER_SIZE)) ? remaining : (idryer::UART_MAX_PAYLOAD - idryer::UART_CONFIG_CHUNK_HEADER_SIZE);
    memcpy(payload.data, jsonStr.c_str() + offset, dataLen);

    // Определяем флаги
    bool isLast = (offset + dataLen >= totalSize);
    uint8_t flags = idryer::UART_FLAG_FRAGMENT;
    if (isLast) {
      flags |= idryer::UART_FLAG_LAST_FRAGMENT;
    }

    // Отправляем чанк
    uint8_t payloadLen = idryer::UART_CONFIG_CHUNK_HEADER_SIZE + dataLen;

    Serial.printf("[UART_TX] Chunk #%u: offset=%u len=%u flags=0x%02X %s\n", chunkIndex, offset, dataLen, flags, isLast ? "[LAST]" : "");
    Serial.printf("  Data: %.*s\n", dataLen, payload.data);

    bool success = uartBridge.sendConfigPushChunk(payload, payloadLen, flags);

    if (!success) {
      Serial.printf("[UART_TX] ERROR: Failed to send chunk %u\n", chunkIndex);
      return false;
    }

    offset += dataLen;
    chunkIndex++;

    // Небольшая задержка между чанками
    delay(10);
  }

  Serial.printf("[UART_TX] Config sent successfully: %u chunks total\n\n", chunkIndex);
  return true;
}

void sendConfigDelta(uint16_t itemId, uint8_t unit) {
  // Находим элемент меню по ID
  if (itemId >= MENU__COUNT) {
    DEBUG_E("sendConfigDelta: Invalid itemId: %u", itemId);
    return;
  }

  const MenuItem &item = g_menu[itemId];
  if (item.type != MN_VALUE && item.type != MN_TOGGLE) {
    DEBUG_W("sendConfigDelta: Item %u is not a value", itemId);
    return; // не value - нечего отправлять
  }

  const ValueSpec &spec = item.u.value;
  if (!spec.ptr) {
    DEBUG_E("sendConfigDelta: Item %u has no pointer", itemId);
    return;
  }

  // Формируем JSON дельты
  StaticJsonDocument<256> doc;

  doc["rev"] = g_config_revision;
  JsonObject vals = doc.createNestedObject("vals");

  // ID как строка
  char idStr[8];
  snprintf(idStr, sizeof(idStr), "%u", itemId);

  bool isPerUnit = isPerUnitMenuItem(item);

  if (isPerUnit) {
    // Per-unit: отправляем массив со всеми значениями (как в full config)
    // Потому что бэк ожидает полный массив для per-unit настроек
    JsonArray arr = vals.createNestedArray(idStr);
    for (uint8_t u = 0; u < menu.units_count; u++) {
      void *ptr = (uint8_t *)spec.ptr + u * item.ee_size;

      switch (spec.vtype) {
      case VT_F32:
        arr.add(*(float *)ptr);
        break;
      case VT_U32:
        arr.add(*(uint32_t *)ptr);
        break;
      case VT_I32:
        arr.add(*(int32_t *)ptr);
        break;
      case VT_U16:
        arr.add(*(uint16_t *)ptr);
        break;
      case VT_U8:
        arr.add(*(uint8_t *)ptr);
        break;
      case VT_BOOL:
        arr.add(*(bool *)ptr);
        break;
      }
    }
  } else {
    // Global: одно значение
    switch (spec.vtype) {
    case VT_F32:
      vals[idStr] = *(float *)spec.ptr;
      break;
    case VT_U32:
      vals[idStr] = *(uint32_t *)spec.ptr;
      break;
    case VT_I32:
      vals[idStr] = *(int32_t *)spec.ptr;
      break;
    case VT_U16:
      vals[idStr] = *(uint16_t *)spec.ptr;
      break;
    case VT_U8:
      vals[idStr] = *(uint8_t *)spec.ptr;
      break;
    case VT_BOOL:
      vals[idStr] = *(bool *)spec.ptr;
      break;
    }
  }

  // Сериализуем в строку
  String jsonStr;
  serializeJson(doc, jsonStr);

  DEBUG_I("Sending config delta: itemId=%u unit=%u rev=%u (%u bytes)", itemId, unit, g_config_revision, jsonStr.length());

  // Отправляем через ConfigPushChunk
  uint16_t totalSize = jsonStr.length();
  uint16_t transferId = millis() & 0xFFFF; // shape JSON ("rev" vs "full") разделяет delta/full

  idryer::UartConfigChunkPayload payload{};
  payload.transferId = transferId;
  payload.totalSize = totalSize;
  payload.chunkIndex = 0;

  memcpy(payload.data, jsonStr.c_str(), totalSize);

  uint8_t flags = idryer::UART_FLAG_ACK_REQ | idryer::UART_FLAG_LAST_FRAGMENT;
  uint8_t payloadLen = idryer::UART_CONFIG_CHUNK_HEADER_SIZE + totalSize;

  bool success = uartBridge.sendConfigPushChunk(payload, payloadLen, flags);

  if (success) {
    DEBUG_I("Config delta sent successfully");
  } else {
    DEBUG_E("Failed to send config delta");
  }
}

// Обработчик legacy ConfigPush (без фрагментации, старый формат)
void handleUartConfigPush(const idryer::UartConfigChunkPayload &payload, uint8_t dataLen, const idryer::UartFrameHeader &header) {
  Serial.printf("\n[UART_RX] ConfigPush %u bytes: %.*s\n", dataLen, dataLen, payload.data);

  // Парсим JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payload.data, dataLen);

  if (error) {
    Serial.printf("[UART_RX] JSON PARSE ERROR: %s\n", error.c_str());
    uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
    return;
  }

  // Проверяем тип команды
  const char *cmd = doc["cmd"];

  if (cmd && strcmp(cmd, "set") == 0) {
    // Команда SET - установить значение настройки
    uint16_t id = doc["id"];
    uint8_t unit = doc.containsKey("unit") ? (uint8_t)doc["unit"] : 0;
    float val = doc["val"];

    Serial.printf("[UART_RX] SET: id=%u unit=%u val=%.2f menu.number_controller will be=%u\n", id, unit, val, unit);

    // Находим элемент меню
    if (id >= MENU__COUNT) {
      DEBUG_E("Invalid menu ID: %u", id);
      uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
      return;
    }

    const MenuItem &item = g_menu[id];
    if (item.type != MN_VALUE && item.type != MN_TOGGLE) {
      DEBUG_E("Menu item is not a value: %u", id);
      uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
      return;
    }

    const ValueSpec &spec = item.u.value;
    if (!spec.ptr) {
      DEBUG_E("Menu item has no pointer: %u", id);
      uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
      return;
    }

    const MenuBinding *binding = findBindingByPtr(spec.ptr);

    if (!binding) {
      DEBUG_E("MenuBinding not found for item %u", id);
      uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
      return;
    }

    bool isPerUnit = binding->scope != SCOPE_GLOBAL;

    if (isPerUnit) {
      if (unit >= NUM_UNITS) {
        DEBUG_E("Invalid unit: %u", unit);
        uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
        return;
      }

      // Временно устанавливаем активный контроллер для per-unit настроек
      uint8_t prevUnit = menu.number_controller;
      menu.number_controller = unit;

      // Применяем через menu_apply_by_bind - вызовет hook автоматически!
      bool success = menu_apply_by_bind(binding->bind, val);

      // Восстанавливаем активный контроллер
      menu.number_controller = prevUnit;

      if (!success) {
        DEBUG_E("menu_apply_by_bind failed for %s", binding->bind);
        uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
        return;
      }
    } else {
      // Global настройка - просто применяем
      if (!menu_apply_by_bind(binding->bind, val)) {
        DEBUG_E("menu_apply_by_bind failed for %s", binding->bind);
        uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
        return;
      }
    }

    DEBUG_I("Value applied via menu_apply_by_bind, rev=%u", g_config_revision);

    // Отправляем ACK
    uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::None);

    // Дельта отправится автоматически через g_config_change_hook!

  } else if (cmd && strcmp(cmd, "invoke") == 0) {
    // Команда INVOKE - вызвать действие
    uint16_t id = doc["id"];

    DEBUG_I("INVOKE command: id=%u", id);

    if (id >= MENU__COUNT) {
      DEBUG_E("Invalid menu ID: %u", id);
      uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
      return;
    }

    const MenuItem &item = g_menu[id];
    if (item.type != MN_ACTION) {
      DEBUG_E("Menu item is not an action: %u", id);
      uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
      return;
    }

    // Отправляем ACK ДО вызова действия: некоторые действия (например, калибровка)
    // блокируют выполнение дольше COMMAND_REPLY_TIMEOUT_MS (700ms), что вызывает
    // ретрансмиссии и многократное выполнение одной команды.
    uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::None);

    // Вызываем действие
    if (item.u.action.invoke) {
      item.u.action.invoke();
      DEBUG_I("Action invoked");
    }

  } else {
    DEBUG_W("Unknown ConfigPush command");
    uartBridge.sendConfigAck(header.sequence, idryer::UartErrCode::InvalidPayload);
  }
}

// =============================================================================
// WEBSOCKET LOCAL ACCESS
// =============================================================================

static idryer::UartWsStatusPayload g_ws_status{};

const idryer::UartWsStatusPayload &getWsStatus() { return g_ws_status; }

bool wsEnabled() { return g_ws_status.state != idryer::UartWsState::Disabled; }

void handleUartWsStatus(const idryer::UartWsStatusPayload &payload, const idryer::UartFrameHeader &header) {
  g_ws_status = payload;
  DEBUG_I("WsStatus: state=%d pin=%u paired=%d/%d", (int)payload.state, payload.pin, payload.pairedCount, payload.maxClients);
}

bool sendWsEnable(bool enable) {
  idryer::UartWsEnablePayload payload{};
  payload.enable = enable ? 1 : 0;
  payload.pin = 0; // PIN не используется (аутентификация через device_token портала)
  bool ok = uartBridge.sendWsEnable(payload);
  DEBUG_I("WsEnable(%d) sent: %s", (int)enable, ok ? "OK" : "FAIL");
  return ok;
}


bool sendWsStatusRequest() {
  bool ok = uartBridge.sendWsStatusRequest();
  DEBUG_I("WsStatusRequest sent: %s", ok ? "OK" : "FAIL");
  return ok;
}

// =============================================================================
// ERROR EVENTS
// =============================================================================

bool sendUartErrorEvent(const ErrorEvent *ev) {
  if (!ev) return false;

  idryer::UartLogPayload logPayload{};
  strncpy(logPayload.severity, errsev_name(ev->severity), sizeof(logPayload.severity) - 1);
  strncpy(logPayload.source, errsrc_name(ev->source), sizeof(logPayload.source) - 1);
  strncpy(logPayload.event, errcode_name(ev->code), sizeof(logPayload.event) - 1);
  strncpy(logPayload.message, ev->msg ? ev->msg : errcode_human(ev->code), sizeof(logPayload.message) - 1);
  logPayload.unitId = ev->ctrl_id;

  return uartBridge.sendLog(logPayload);
}
