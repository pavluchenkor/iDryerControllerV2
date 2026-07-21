// src/menu_callbacks_user.cpp
// #pragma once
#include "../version.h"
#include "HX711/HX711.h"
#include "claiming/claiming.h"
#include "configuration.h"
#include "controller/control.h"
#include "hardware/hardware.h"
#include "leds/leds.h"
#include "menu_eeprom.h"
#include "menu_eeprom_io.h"
#include "menu_state.h"
#include "service_screen/service_screen_state.h"
#include "uart/uart_manager.h"
#include <Arduino.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_TRACE // LOG_LEVEL_TRACE
#define LOG_TAG "MENU_CB"
#include "debug_log.h"

extern uint8_t gscr_active_unit;
extern ScreenState gscr_units[NUM_UNITS];

extern MenuState menu;
extern DryerController *controllers[NUM_UNITS];

extern "C" uint8_t menu_get_active_controller(void);
static inline uint16_t nz_or(uint16_t v, uint16_t fb) { return v ? v : fb; }

static void start_drying_from(uint16_t tempC, uint16_t minutes, const char *tag) {
  tempC = nz_or(tempC, 60);
  minutes = nz_or(minutes, 30);

  uint8_t i = menu_get_active_controller();
  if (i >= NUM_UNITS || !controllers[i]) {
    DEBUG_W("[PRESET] invalid controller index %u", i);
    return;
  }

  // ✅ Инкремент счетчика перенесен в DryerController::startDrying()
  // Это гарантирует инкремент независимо от источника команды (меню, UART, и т.д.)
  controllers[i]->requestMode(DryerMode::Drying, tempC, minutes);
  DEBUG_I("[PRESET] %s -> U%u T=%uC %umin", tag, i, (unsigned)tempC, (unsigned)minutes);
}


extern "C" {

#define DEFINE_PRESET_START(name, tfld, tfmin)                                                                                                                                                                                                                                                                                                                                                                 \
  void name() { start_drying_from((uint16_t)menu.tfld, (uint16_t)menu.tfmin, #name); }
#include "menu_presets_autogen.h"


#if 0 // CLAIM ВРЕМЕННО ОТКЛЮЧЁН
void start_claim(void) {
  const uint32_t now = millis();
  claim_begin(now);

  if (!hasLink() || !uartLinkReady()) {
    DEBUG_W("[CLAIM] Link not ready, showing error");
    idryer::UartClaimStatusPayload err{};
    err.status = idryer::UartClaimStatus::Error;
    claim_on_status(err, now);
    return;
  }

  if (!sendUartClaimStart()) {
    DEBUG_W("[CLAIM] Failed to send ClaimStart");
    idryer::UartClaimStatusPayload err{};
    err.status = idryer::UartClaimStatus::Error;
    claim_on_status(err, now);
  }
}
#endif // CLAIM ВРЕМЕННО ОТКЛЮЧЁН

void start_profile(void) {
  uint8_t i = menu_get_active_controller();
  if (i >= NUM_UNITS || !controllers[i]) {
    DEBUG_W("[PROFILE] invalid controller index %u", i);
    return;
  }

  // ✅ Инкремент счетчика перенесен в DryerController::startProfile()
  // Это гарантирует инкремент независимо от источника команды (меню, UART, и т.д.)

  uint8_t startStage = menu.start_stage_by_number[i];
  if (startStage < 1 || startStage > 10) {
    startStage = 1;
  }
  startStage--; // Конвертируем из 1-based в 0-based для контроллера

  // Собираем ProfileData из меню (конвертируем минуты в секунды)
  ProfileData prof;
  prof.stageCount = 10;

  prof.stages[0].holdTempC = (uint16_t)menu.profile_stage_01_temp[i];
  prof.stages[0].holdSeconds = (uint16_t)menu.profile_stage_01_time[i] * 60;
  prof.stages[0].rampSeconds = (uint16_t)menu.profile_stage_01_ramps_time[i] * 60;

  prof.stages[1].holdTempC = (uint16_t)menu.profile_stage_02_temp[i];
  prof.stages[1].holdSeconds = (uint16_t)menu.profile_stage_02_time[i] * 60;
  prof.stages[1].rampSeconds = (uint16_t)menu.profile_stage_02_ramps_time[i] * 60;

  prof.stages[2].holdTempC = (uint16_t)menu.profile_stage_03_temp[i];
  prof.stages[2].holdSeconds = (uint16_t)menu.profile_stage_03_time[i] * 60;
  prof.stages[2].rampSeconds = (uint16_t)menu.profile_stage_03_ramps_time[i] * 60;

  prof.stages[3].holdTempC = (uint16_t)menu.profile_stage_04_temp[i];
  prof.stages[3].holdSeconds = (uint16_t)menu.profile_stage_04_time[i] * 60;
  prof.stages[3].rampSeconds = (uint16_t)menu.profile_stage_04_ramps_time[i] * 60;

  prof.stages[4].holdTempC = (uint16_t)menu.profile_stage_05_temp[i];
  prof.stages[4].holdSeconds = (uint16_t)menu.profile_stage_05_time[i] * 60;
  prof.stages[4].rampSeconds = (uint16_t)menu.profile_stage_05_ramps_time[i] * 60;

  prof.stages[5].holdTempC = (uint16_t)menu.profile_stage_06_temp[i];
  prof.stages[5].holdSeconds = (uint16_t)menu.profile_stage_06_time[i] * 60;
  prof.stages[5].rampSeconds = (uint16_t)menu.profile_stage_06_ramps_time[i] * 60;

  prof.stages[6].holdTempC = (uint16_t)menu.profile_stage_07_temp[i];
  prof.stages[6].holdSeconds = (uint16_t)menu.profile_stage_07_time[i] * 60;
  prof.stages[6].rampSeconds = (uint16_t)menu.profile_stage_07_ramps_time[i] * 60;

  prof.stages[7].holdTempC = (uint16_t)menu.profile_stage_08_temp[i];
  prof.stages[7].holdSeconds = (uint16_t)menu.profile_stage_08_time[i] * 60;
  prof.stages[7].rampSeconds = (uint16_t)menu.profile_stage_08_ramps_time[i] * 60;

  prof.stages[8].holdTempC = (uint16_t)menu.profile_stage_09_temp[i];
  prof.stages[8].holdSeconds = (uint16_t)menu.profile_stage_09_time[i] * 60;
  prof.stages[8].rampSeconds = (uint16_t)menu.profile_stage_09_ramps_time[i] * 60;

  prof.stages[9].holdTempC = (uint16_t)menu.profile_stage_10_temp[i];
  prof.stages[9].holdSeconds = (uint16_t)menu.profile_stage_10_time[i] * 60;
  prof.stages[9].rampSeconds = (uint16_t)menu.profile_stage_10_ramps_time[i] * 60;

  controllers[i]->startProfile(prof, startStage);
  DEBUG_I("[PROFILE] U%u started from stage %u (1-based: %u)", i, startStage, startStage + 1);
}

void set_active_controller() { menu.number_controller = menu.number_controller > menu.units_count - 1 ? menu.units_count - 1 : menu.number_controller; }

void units_count_set(void *ctx) {
  const uint8_t MAX_UNITS_HW = calcMaxUnits();

  // Ограничиваем значение по аппаратной конфигурации
  if (menu.units_count > MAX_UNITS_HW) {
    DEBUG_W("[units_count_set] Значение %u превышает максимум %u, корректируем", menu.units_count, MAX_UNITS_HW);
    menu.units_count = MAX_UNITS_HW;
    // ВАЖНО: сохраняем исправленное значение в EEPROM (адрес EE_OFF_UNITS_COUNT menu_bindings.cpp)
    ee_store_field(EE_OFF_UNITS_COUNT, menu.units_count);
  }

  // Корректируем активный контроллер если он вышел за пределы
  if (menu.number_controller >= menu.units_count) {
    menu.number_controller = menu.units_count - 1;
    DEBUG_I("[units_count_set] Скорректирован number_controller -> %u", menu.number_controller);
  }
}

void start_storage(void) {
  uint8_t i = menu_get_active_controller();
  if (i >= NUM_UNITS) i = 0;

  // ✅ Инкремент счетчика перенесен в DryerController::startStorage()
  // Это гарантирует инкремент независимо от источника команды (меню, UART, и т.д.)

  uint16_t tC = (uint16_t)menu.storage_temp[i];
  uint8_t rh = (uint8_t)menu.storage_hum[i];
  bool byHum = menu.storage_hum_priority[i]; // true=по влажности, false=по температуре

  controllers[i]->requestMode(DryerMode::Storage, tC, /*minutes*/ 0, rh, byHum);
  DEBUG_I("[PRESET] STORAGE -> U%u T=%uC RH=%u%% byHum=%d", i, (unsigned)tC, (unsigned)rh, (int)byHum);
}

extern "C" uint8_t menu_get_active_controller(void) { return (menu.number_controller < NUM_UNITS) ? menu.number_controller : 0; }

void start_drying() {
  DEBUG_I("START_DRYING");
  const uint8_t i = menu_get_active_controller();
  start_drying_from((uint16_t)menu.dry_temp[i], menu.dry_time[i], "start_drying");
}

void start_storage_hum(void) {
  uint16_t tempC = 40;
  uint8_t hum = 50;
  bool byHum = true;
  DEBUG_I("START_STORAGE_HUM");
  const uint8_t i = menu_get_active_controller();
  if (i < NUM_UNITS) {
    controllers[i]->requestMode(DryerMode::Storage, tempC, /*minutes=*/0, hum, byHum);
    DEBUG_I("start_storage on controller %u\n", i);
  } else {
    DEBUG_I("invalid controller index %u\n", i);
  }
}

void pid_autotune_heater(void * /*arg*/) {
  const uint8_t u = menu_get_active_controller();
  if (u >= NUM_UNITS || !controllers[u]) return;

  DEBUG_E("pid_autotune_heater u=%d", u);
  float t = (float)menu.heater_max_temp[u] * 0.8f;
  if (t <= 0) t = 100; // запасной дефолт

  controllers[u]->startPidAutoTune((uint16_t)lroundf(t),
                                   /*cycles*/ 10, AutoTuneTarget::HeaterTemp,
                                   /*band*/ 2.0f,
                                   /*power*/ 1.0f);
}

void pid_autotune_chamber(void * /*arg*/) {
  const uint8_t u = menu_get_active_controller();
  if (u >= NUM_UNITS || !controllers[u]) return;

  DEBUG_E("pid_autotune_chamber u=%d", u);

  float t = (float)menu.air_max_temp[u] * 0.9f;
  if (t <= 0) t = 60;

  controllers[u]->startPidAutoTune((uint16_t)lroundf(t),
                                   /*cycles*/ 10, AutoTuneTarget::AirTemp,
                                   /*band*/ 2.0f,
                                   /*power*/ 1.0f);
}

void stop_all(void) {
  // iDryer0.requestMode(DryerMode::Idle);
  // Serial.println("[MENU] stop_all -> Idle");
}

extern "C" void on_servo_preview(void *ptr) {
  const uint8_t i = menu_get_active_controller();

  // DEBUG_E("on_servo_preview controllers = %d", i);
  if (i >= NUM_UNITS || !controllers[i]) return;
  // угол берём из массива для активного контроллера
  if (ptr == (void *)&menu.servo_closed_angle[i]) {
    controllers[i]->previewServoAngle(menu.servo_closed_angle[i]);
    controllers[i]->setServoLimits(menu.servo_closed_angle[i], menu.servo_open_angle[i]);
    DEBUG_W("on_servo_preview controller = d%   servo_closed = %d", i, menu.servo_closed_angle[i]);
  } else if (ptr == (void *)&menu.servo_open_angle[i]) {
    controllers[i]->previewServoAngle(menu.servo_open_angle[i]);
    controllers[i]->setServoLimits(menu.servo_closed_angle[i], menu.servo_open_angle[i]);
    DEBUG_W("on_servo_preview controller = d%    servo_open = %d", i, menu.servo_open_angle[i]);
  }
}

static inline void apply_heater_pid(uint8_t u) {
  if (u >= NUM_UNITS) return;
  controllers[u]->setHeaterPid(u, PID_ACT_HEATER, menu.pid_kp_heater[u], menu.pid_ki_heater[u], menu.pid_kd_heater[u],
                               /*minOut*/ 0.0f,
                               /*maxOut*/ 100.0f,
                               /*sample*/ PID_UPDATE_INTERVAL_S,
                               /*gain_multiplier*/ menu.pid_gain_heater[u]);
  DEBUG_I("[PID][HEATER][U%u] Kp=%.3f Ki=%.3f Kd=%.3f", u, menu.pid_kp_heater[u], menu.pid_ki_heater[u], menu.pid_kd_heater[u]);
}

static inline void apply_chamber_pid(uint8_t u) {
  if (u >= NUM_UNITS) return;
  controllers[u]->setAirPid(u, PID_ACT_AIR, menu.pid_kp_chamber[u], menu.pid_ki_chamber[u], menu.pid_kd_chamber[u],
                            /*minOut*/ 0.0f,
                            /*maxOut*/ menu.heater_max_temp[u],
                            /*sample*/ PID_UPDATE_INTERVAL_S,
                            /*gain_multiplier*/ menu.pid_gain_chamber[u]);
  DEBUG_I("[PID][AIR][U%u] Kp=%.3f Ki=%.3f Kd=%.3f", u, menu.pid_kp_chamber[u], menu.pid_ki_chamber[u], menu.pid_kd_chamber[u]);
}

void pid_recalc_heater(void *arg) {
  DEBUG_I("pid_recalc_heater arg=%p", arg);
  if (arg) {
    uint8_t u = (uint8_t)(uintptr_t)arg;
    apply_heater_pid(u);
  } else {
    for (uint8_t u = 0; u < NUM_UNITS; ++u)
      apply_heater_pid(u);
  }
}

void pid_recalc_chamber(void *arg) {
  DEBUG_I("pid_recalc_chamber arg=%p", arg);
  if (arg) {
    uint8_t u = (uint8_t)(uintptr_t)arg;
    apply_chamber_pid(u);
  } else {
    for (uint8_t u = 0; u < NUM_UNITS; ++u)
      apply_chamber_pid(u);
  }
}

void pid_recalc_gain_heater(void *arg) {
  DEBUG_I("pid_recalc_gain_heater arg=%p", arg);
  if (arg) {
    uint8_t u = (uint8_t)(uintptr_t)arg;
    if (u < NUM_UNITS && controllers[u]) controllers[u]->updateHeaterGain(menu.pid_gain_heater[u]);
  } else {
    for (uint8_t u = 0; u < NUM_UNITS; ++u)
      if (controllers[u]) controllers[u]->updateHeaterGain(menu.pid_gain_heater[u]);
  }
}

void pid_recalc_gain_chamber(void *arg) {
  DEBUG_I("pid_recalc_gain_chamber arg=%p", arg);
  if (arg) {
    uint8_t u = (uint8_t)(uintptr_t)arg;
    if (u < NUM_UNITS && controllers[u]) controllers[u]->updateAirGain(menu.pid_gain_chamber[u]);
  } else {
    for (uint8_t u = 0; u < NUM_UNITS; ++u)
      if (controllers[u]) controllers[u]->updateAirGain(menu.pid_gain_chamber[u]);
  }
}

void calib_kg_1() {
  if (hx711MultiPtr) hx711Multi.offsetSetupMulti(0);
}
void calib_kg_2() {
  if (hx711MultiPtr) hx711Multi.offsetSetupMulti(1);
}
void calib_kg_3() {
  if (hx711MultiPtr) hx711Multi.offsetSetupMulti(2);
}
void calib_kg_4() {
  if (hx711MultiPtr) hx711Multi.offsetSetupMulti(3);
}

void calib_zero1() {
  if (hx711MultiPtr) hx711Multi.zeroSetupMulti(0);
}
void calib_zero2() {
  if (hx711MultiPtr) hx711Multi.zeroSetupMulti(1);
}
void calib_zero3() {
  if (hx711MultiPtr) hx711Multi.zeroSetupMulti(2);
}
void calib_zero4() {
  if (hx711MultiPtr) hx711Multi.zeroSetupMulti(3);
}
void scales_count_set() {
  if (hx711MultiPtr) hx711Multi.setNumSensors(menu.scales_count);
}

// =============================================================================
// WEBSOCKET LOCAL ACCESS
// =============================================================================

void ws_toggle(void * /*ptr*/) {
  bool en = menu.ws_enabled;
  if (en && (!hasLink() || !uartLinkReady())) {
    DEBUG_W("[WS] Link not ready, reverting toggle");
    menu.ws_enabled = false;
    return;
  }

  sendWsEnable(en);
  DEBUG_I("[WS] toggle -> %s", en ? "ON" : "OFF");
}

} // extern "C
