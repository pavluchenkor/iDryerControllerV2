#include "HX711/HX711.h"
#include "RFID/RfidManager.h"
#include "RFID/openprinttag_test_payload.h"
#include "UI/menu_ui.h"
#include "analog_buttons/analog_buttons.h"
#include "configuration.h"
#include "controller/control.h"
#include "error/error_bus.h"
#include "error/error_eelog.h"
#include "error/error_overlay.h"
#include "error/error_post.h"
#include "error/error_table.h"
#include "hardware/hardware.h"
#include "leds/leds.h"
#include "menu/menu_eeprom.h"
#include "menu/menu_ids.h"
#include "menu/menu_state.h"
#include "menu/menu_state_dump.h"
#include "menu/menu_types.h"
#include "sensor/Sensor.h"
#include "sensor/Sht31Sensor.h"
#include "sensor/ThermistorSensor.h"
#include "service_screen/service_screen.h"
#include "service_screen/service_screen_state.h"
#include "session/session_counters.h"
#include "uart/uart_manager.h"
#include "uart/ota_proxy_rp.h"
#include "version.h"
#include <ADCbuttons.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <ISRbutton.h>
#include <ISRencoder.h>
#include <SHT31.h>
#include <Servo.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WorkTimeCounter.h>
#include <ema_filter.h>
#include <uart/uart_bridge.h>
#include <thermistor.h>

#define KASYAK_FINDER 1
// КРИТИЧНО: Снижен уровень логов до ERROR, чтобы не тормозить loop()
// DEBUG_I/DEBUG_W блокируют Serial.print() на 1-5мс → RX FIFO переполняется
#define LOG_LEVEL LOG_LEVEL_INFO
#include "debug_log.h"
#define LOG_TAG "MAIN"

// #define TEST_FAN_ONLY
// #define TEST_HEATER_ONLY
// #define TEST_FAN_AND_HEATER

// Runtime gate: true = IDLE (все логи), false = активный режим (только WARN+/OPR)
volatile bool g_verbose_logs = true;

#ifndef EE_EEPROM_SIZE
#define EE_EEPROM_SIZE 4096 // фиксированный размер EEPROM, чтобы не сдвигать системную зону (таймер и калибровки)
#endif

//* ---------------- Serial command ------------------
// #define CMD_BUFFER_SIZE 64
// #define CMD_DELIMITER '\n'

// class SerialCommands {
// private:
//   char buffer[CMD_BUFFER_SIZE];
//   uint8_t bufferIndex = 0;

// public:
//   void init() { Serial.begin(115200); }

//   void update() {
//     while (Serial.available()) {
//       char c = Serial.read();

//       if (c == CMD_DELIMITER) {
//         buffer[bufferIndex] = '\0';
//         processCommand(buffer);
//         bufferIndex = 0;
//       } else if (bufferIndex < CMD_BUFFER_SIZE - 1) {
//         buffer[bufferIndex++] = c;
//       }
//     }
//   }

// private:
//   void processCommand(const char *cmd) {
//     // Пример команд:
//     // temp:N:V - установить температуру N-го блока в V градусов
//     // get:temps - получить все температуры
//     // error:clear - очистить лог ошибок

//     if (strncmp(cmd, "temp:", 5) == 0) {
//       int unit, temp;
//       if (sscanf(cmd + 5, "%d:%d", &unit, &temp) == 2) {
//         Serial.printf("Set unit %d temp to %d\n", unit, temp);
//         //  код установки температуры
//       }
//     }
//     if (strncmp(cmd, "scale:", 6) == 0) {
//       int scale;
//       if (sscanf(cmd + 6, "%d", &scale) == 1) {
//         Serial.printf("Set scale %d\n", scale);
//         digitalWrite(SERVO2, (scale >> 0) & 1);      // бит 0
//         digitalWrite(THERMISTOR2, (scale >> 1) & 1); // бит 1
//       }
//     }
//     if (strncmp(cmd, "servo:", 6) == 0) {
//       int unit, state;
//       if (sscanf(cmd + 6, "%d:%d", &unit, &state) == 2) {
//         Serial.printf("Set unit %d servo is %s\n", unit, state ? "OPEN" : "CLOSED");
//         if (unit >= 0 && unit < menu.units_count) {
//           controllers[unit]->setFlapOpenBool(bool(state));
//         }
//       }
//     } else if (strcmp(cmd, "get:temps") == 0) {
//       // код чтения температур
//       Serial.println("Temperatures:");
//       for (int i = 0; i < 3; i++) {
//         Serial.printf("Unit %d: %.1fC\n", i, (double)0.0f);
//       }
//     } else if (strcmp(cmd, "error:clear") == 0) {
//       errlog_clear();
//       Serial.println("Error log cleared");
//     }
//   }
// };

// SerialCommands cmd;

// * MENU -----------------
MenuUI ui;

extern "C" uint8_t ui_lang;
static uint32_t ui_block_until = 0;
static bool ui_armed = false;

#include "menu/menu_ids.h" // для констант LANG_*
uint8_t ui_lang = LANG_RU; // или LANG_EN по умолчанию

// ----------------- ENCODER & BUTTONS -----------------
// Объекты всегда существуют; tick()/usage только когда hasScreen()
#define ENCODER_S1 (TRA)
#define ENCODER_S2 (TRB)
#define ENCODER_KEY (PSH)
// Энкодер и кнопка создаются только если Port3 = SCREEN
ISRencoder *enc = nullptr;
ISRbutton *enc_btn = nullptr;
ADCbuttons adc_btns(CON, 4095, 450);

// ----------------- u8g2 OLED -----------------
U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ----------------- WorkHoursCounter -----------------
WorkTimeCounter wtc;

// ----------------- UART -----------------
#include <SerialPIO.h>
#include <hal/hal_arduino.h>
using namespace idryer::hal;

// Runtime UART: оба объекта существуют, выбор в setup() через uartSerialPtr
SerialPIO serialPort3(1, 2, 4096);      // TX=GPIO1, RX=GPIO2 для Port3 LNK (PIO-based). RX-буфер 4096: OTA-proxy chunk=4КБ, переживает паузы loop без overrun (CRC mismatch)
ArduinoSerial uartSerial1(Serial1);     // UART0 — Port3 LNK (закомментирован в setup)
ArduinoSerial uartSerial2(Serial2);     // UART1 — Port2 LNK
ArduinoSerial uartSerial3(serialPort3); // PIO UART для Port3
ArduinoSerial *uartSerialPtr = nullptr;

void serviceTasks(uint32_t now);
void handlePendingUartCommands();

void waitSerial(uint32_t timeout_ms = 2000) {
  // [TEMP-DEBUG] для поимки boot-output: расширили до 60 сек чтобы успеть
  // подключить python serial после power-cycle. Вернуть обратно после отладки.
  timeout_ms = 60000;
  uint32_t t0 = millis();
  while (!Serial) {
    if (millis() - t0 > timeout_ms) break;
    delay(10);
  }
  delay(200);
}


static void errorReportTick(uint32_t now);

// ---------- RFID state (file-scope — нужен в loop() и handlePendingUartCommands()) ----------
static RfidTag  g_rfidLastTag[RFID_MAX_READERS]     = {};
static bool     g_rfidTagPresent[RFID_MAX_READERS]  = {};
constexpr size_t  RFID_DATA_SIZE  = 888;
constexpr uint8_t RFID_FRAG_SIZE  = 163;
constexpr uint8_t RFID_BLOCKS_PER_STEP = 1;
constexpr uint8_t RFID_MAX_BLOCK_SIZE = 16;
enum class RfidTransferMode : uint8_t { Sync, Async };
enum class RfidVerifyMode : uint8_t { None, Header32, Full };

struct RfidReadJob {
  bool active = false;
  uint8_t readerId = 0xFF;
  uint8_t unitId = 0xFF;
  RfidTag tag{};
  size_t readSize = 0;
  size_t nextOffset = 0;
  uint32_t startedAtMs = 0;
  uint32_t maxStepMs = 0;
  uint8_t data[RFID_DATA_SIZE] = {};
};

struct RfidWriteJob {
  bool active = false;
  bool verifyPhase = false;
  uint8_t readerId = 0xFF;
  uint8_t unitId = 0xFF;
  RfidTag tag{};
  size_t writeSize = 0;
  size_t nextOffset = 0;
  uint32_t startedAtMs = 0;
  uint32_t maxStepMs = 0;
  RfidVerifyMode verifyMode = RfidVerifyMode::None;
  uint8_t data[RFID_DATA_SIZE] = {};
};

struct RfidPreviewJob {
  bool active = false;
  uint8_t readerId = 0xFF;
  RfidTag tag{};
  size_t previewSize = 0;
  size_t nextOffset = 0;
  uint32_t startedAtMs = 0;
  uint32_t maxStepMs = 0;
  uint8_t data[32] = {};
};

// Staging-буфер под входящие фрагменты RfidWriteData (0x1B).
// Активируется командой WriteRfid (0x08) с указанием ожидаемого размера;
// далее приходящие фрагменты складываются подряд, по FLAG_LAST_FRAGMENT
// запускается RfidWriteJob для реальной записи на метку.
struct RfidWriteStaging {
  bool active = false;
  uint8_t readerId = 0xFF;
  uint8_t unitId = 0xFF;
  RfidVerifyMode verifyMode = RfidVerifyMode::Header32;
  size_t expectedSize = 0;
  size_t receivedSize = 0;
  uint32_t armedAtMs = 0;
  uint8_t data[RFID_DATA_SIZE] = {};
};

static RfidReadJob g_rfidReadJobs[RFID_MAX_READERS] = {};
static RfidWriteJob g_rfidWriteJobs[RFID_MAX_READERS] = {};
static RfidPreviewJob g_rfidPreviewJobs[RFID_MAX_READERS] = {};
static RfidWriteStaging g_rfidWriteStaging[RFID_MAX_READERS] = {};
constexpr uint32_t RFID_WRITE_STAGING_TIMEOUT_MS = 10000;

// Sync-флаг: ставится в UART-handler при получении Command WriteRfid (ещё до armed staging),
// блокирует PN5180-поллинг на этом ридере до момента armed или по таймауту.
static volatile bool g_rfidWritePending[RFID_MAX_READERS] = {};
static volatile uint32_t g_rfidWritePendingUntilMs[RFID_MAX_READERS] = {};
static uint16_t g_rfidTestWeightG = OpenPrintTagTest::INITIAL_ACTUAL_WEIGHT_G;
static uint32_t g_rfidTestRuns = 0;
static bool g_rfidSelfTestArmed[RFID_MAX_READERS] = {};
static bool g_rfidSelfTestDone[RFID_MAX_READERS] = {};
static uint32_t g_rfidSelfTestDetectedAtMs[RFID_MAX_READERS] = {};
static bool g_rfidSelfTestLatched[RFID_MAX_READERS] = {};
static RfidTag g_rfidSelfTestLatchedTag[RFID_MAX_READERS] = {};
static uint8_t g_rfidNoTagStreak[RFID_MAX_READERS] = {};
static uint32_t g_rfidNoTagSinceMs[RFID_MAX_READERS] = {};
static uint32_t g_rfidNextPollAtMs[RFID_MAX_READERS] = {};
constexpr uint32_t RFID_SELF_TEST_SETTLE_MS = 1500;
constexpr uint16_t RFID_SELF_TEST_WEIGHT_STEP_G = 5;
constexpr uint8_t RFID_TAG_LOST_STREAK = 4;
constexpr uint32_t RFID_TAG_LOST_MIN_MS = 120;
constexpr uint32_t RFID_POLL_INTERVAL_MS = 12;

static size_t rfidTagReadableBytes(const RfidTag &tag);

static bool rfidUidEqual(const RfidTag &a, const RfidTag &b) {
  if (a.uidLen != b.uidLen) return false;
  for (uint8_t i = 0; i < a.uidLen; i++)
    if (a.uid[i] != b.uid[i]) return false;
  return true;
}

static bool rfidSameLatchedSelfTestTag(uint8_t readerId, const RfidTag &tag) {
  if (readerId >= RFID_MAX_READERS) return false;
  if (!g_rfidSelfTestLatched[readerId]) return false;
  return rfidUidEqual(g_rfidSelfTestLatchedTag[readerId], tag);
}

static uint8_t rfidTagStartPage(const RfidTag &tag) {
  return tag.firstUserBlock;
}

static uint8_t rfidTagBlockSize(const RfidTag &tag) {
  return tag.blockSize > 0 ? tag.blockSize : 4;
}

static bool rfidTagBlockSizeSupported(const RfidTag &tag) {
  const uint8_t blockSize = rfidTagBlockSize(tag);
  return blockSize > 0 && blockSize <= RFID_MAX_BLOCK_SIZE;
}

// Маппинг user-block индекса на физический pageNo для READ-операций.
// Для MIFARE Classic 1K перескакиваем sector trailer (каждый 4-й блок),
// чтобы readRaw не дергал лишний sector switch и не читал блоки дважды.
// idx=0..47 → blocks 0, 1, 2, 4, 5, 6, 8, ..., 62 (включая block 0 = UID+manufacturer).
static uint8_t rfidTagPageNoAt(const RfidTag &tag, size_t userBlockIdx) {
  if (tag.type == RfidTagType::MIFARE_CLASSIC_1K) {
    return static_cast<uint8_t>((userBlockIdx / 3) * 4 + (userBlockIdx % 3));
  }
  return static_cast<uint8_t>(rfidTagStartPage(tag) + userBlockIdx);
}

// Маппинг user-block индекса для WRITE/verify-операций.
// Для MIFARE Classic 1K дополнительно пропускаем block 0 (manufacturer, read-only),
// чтобы write и verify-read адресовали одни и те же физические блоки.
// idx=0..46 → blocks 1, 2, 4, 5, 6, 8, ..., 62 (47 writable blocks = 752 Б).
static uint8_t rfidTagWritePageNoAt(const RfidTag &tag, size_t userBlockIdx) {
  if (tag.type == RfidTagType::MIFARE_CLASSIC_1K) {
    if (userBlockIdx < 2) {
      return static_cast<uint8_t>(userBlockIdx + 1);
    }
    const size_t adjIdx = userBlockIdx - 2;
    return static_cast<uint8_t>(4 + (adjIdx / 3) * 4 + (adjIdx % 3));
  }
  return static_cast<uint8_t>(rfidTagStartPage(tag) + userBlockIdx);
}

static void rfidStartPreviewJob(uint8_t readerId, const RfidTag &tag) {
  if (readerId >= RFID_MAX_READERS) return;

  RfidPreviewJob &job = g_rfidPreviewJobs[readerId];
  job = {};
  job.readerId = readerId;
  job.tag = tag;
  job.previewSize = rfidTagReadableBytes(tag);
  if (job.previewSize > sizeof(job.data)) job.previewSize = sizeof(job.data);
  if (job.previewSize == 0 || !rfidTagBlockSizeSupported(tag)) {
    return;
  }

  job.active = true;
  job.startedAtMs = millis();
}

static void rfidFlushPreviewJob(RfidPreviewJob &job) {
  const uint8_t previewStartPage = rfidTagStartPage(job.tag);
  const uint8_t previewBlockSize = rfidTagBlockSize(job.tag);
  Serial.printf("[RFID] preview blocks %u-%u:",
                (unsigned)previewStartPage,
                (unsigned)(previewStartPage + ((job.previewSize + previewBlockSize - 1) / previewBlockSize) - 1));
  for (size_t b = 0; b < job.previewSize; b++) {
    if ((b % previewBlockSize) == 0) {
      Serial.printf("\n  b%02u:", previewStartPage + (b / previewBlockSize));
    }
    Serial.printf(" %02X", job.data[b]);
  }
  Serial.println();
  DEBUG_I("[RFID] preview read reader=%u bytes=%u in %lums maxStep=%lums",
          job.readerId,
          (unsigned)job.previewSize,
          (unsigned long)(millis() - job.startedAtMs),
          (unsigned long)job.maxStepMs);
}

static void rfidServicePreviewJobs() {
  for (uint8_t readerId = 0; readerId < RFID_MAX_READERS; readerId++) {
    RfidPreviewJob &job = g_rfidPreviewJobs[readerId];
    if (!job.active) continue;

    if (g_rfidReadJobs[readerId].active || g_rfidWriteJobs[readerId].active) {
      continue;
    }
    if (!g_rfidTagPresent[readerId] || !rfidUidEqual(job.tag, g_rfidLastTag[readerId])) {
      job.active = false;
      continue;
    }

    const uint8_t blockSize = rfidTagBlockSize(job.tag);
    if (!rfidTagBlockSizeSupported(job.tag)) {
      job.active = false;
      continue;
    }

    if (job.nextOffset >= job.previewSize) {
      rfidFlushPreviewJob(job);
      job.active = false;
      continue;
    }

    uint32_t stepStartedAt = millis();
    uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
    uint8_t pageNo = rfidTagPageNoAt(job.tag, job.nextOffset / blockSize);
    RfidStatus st = rfidManager.read(readerId, job.tag, pageNo, block, blockSize);
    uint32_t stepMs = millis() - stepStartedAt;
    if (stepMs > job.maxStepMs) job.maxStepMs = stepMs;
    if (st != RfidStatus::OK) {
      DEBUG_W("[RFID] preview read failed reader=%u page=%u st=%u",
              readerId, pageNo, (uint8_t)st);
      job.active = false;
      continue;
    }

    size_t copyLen = job.previewSize - job.nextOffset;
    if (copyLen > blockSize) copyLen = blockSize;
    memcpy(job.data + job.nextOffset, block, copyLen);
    job.nextOffset += blockSize;
  }
}

/**
 * @brief Читает произвольный диапазон страниц синхронно с замером худшего шага.
 */
static bool rfidReadRangeSync(uint8_t readerId, const RfidTag &tag, uint8_t startPage,
                              uint8_t *data, size_t size, uint32_t &maxStepMs) {
  if (!data || !rfidTagBlockSizeSupported(tag)) return false;
  const uint8_t blockSize = rfidTagBlockSize(tag);
  const bool isMfc = (tag.type == RfidTagType::MIFARE_CLASSIC_1K);
  maxStepMs = 0;
  uint8_t pageNo = startPage;
  for (size_t offset = 0; offset < size; pageNo++) {
    if (isMfc && (pageNo % 4 == 3)) continue; // skip trailer blocks
    watchdog_update();
    uint32_t stepStartedAt = millis();
    uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
    RfidStatus st = rfidManager.read(readerId, tag, pageNo, block, blockSize);
    uint32_t stepMs = millis() - stepStartedAt;
    if (stepMs > maxStepMs) maxStepMs = stepMs;
    if (st != RfidStatus::OK) {
      DEBUG_W("[RFIDTEST] read failed reader=%u page=%u st=%u",
              readerId, pageNo, (uint8_t)st);
      return false;
    }

    size_t copyLen = size - offset;
    if (copyLen > blockSize) copyLen = blockSize;
    memcpy(data + offset, block, copyLen);
    offset += blockSize;
  }
  return true;
}

/**
 * @brief Записывает произвольный диапазон страниц синхронно с замером худшего шага.
 */
static bool rfidWriteRangeSync(uint8_t readerId, const RfidTag &tag, uint8_t startPage,
                               const uint8_t *data, size_t size, uint32_t &maxStepMs) {
  if (!data || !rfidTagBlockSizeSupported(tag)) return false;
  const uint8_t blockSize = rfidTagBlockSize(tag);
  const bool isMfc = (tag.type == RfidTagType::MIFARE_CLASSIC_1K);
  maxStepMs = 0;
  uint8_t pageNo = startPage;
  for (size_t offset = 0; offset < size; pageNo++) {
    if (isMfc && (pageNo % 4 == 3)) continue; // skip trailer blocks
    watchdog_update();
    uint32_t stepStartedAt = millis();
    uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
    size_t copyLen = size - offset;
    if (copyLen > blockSize) copyLen = blockSize;
    memcpy(block, data + offset, copyLen);

    RfidStatus st = rfidManager.write(readerId, tag, pageNo, block, blockSize);
    uint32_t stepMs = millis() - stepStartedAt;
    if (stepMs > maxStepMs) maxStepMs = stepMs;
    if (st != RfidStatus::OK) {
      DEBUG_W("[RFIDTEST] write failed reader=%u page=%u st=%u",
              readerId, pageNo, (uint8_t)st);
      return false;
    }
    offset += blockSize;
  }
  return true;
}

/**
 * @brief Находит первый ридер с текущей меткой.
 */
static bool rfidFindPresentReader(uint8_t &readerId) {
  for (uint8_t r = 0; r < RFID_MAX_READERS; r++) {
    if (r >= rfidManager.readerCount()) break;
    if (!rfidManager.isAvailable(r)) continue;
    if (!g_rfidTagPresent[r]) continue;
    readerId = r;
    return true;
  }
  return false;
}

/**
 * @brief Читает actual weight из тестового OpenPrintTag-образа.
 */
static uint16_t rfidTestReadWeight(const uint8_t *data, size_t size) {
  if (!data || size <= OpenPrintTagTest::ACTUAL_WEIGHT_VALUE_OFFSET + 1) return 0;
  return (uint16_t(data[OpenPrintTagTest::ACTUAL_WEIGHT_VALUE_OFFSET]) << 8) |
         uint16_t(data[OpenPrintTagTest::ACTUAL_WEIGHT_VALUE_OFFSET + 1]);
}

/**
 * @brief Записывает actual weight в тестовый OpenPrintTag-образ.
 */
static void rfidTestWriteWeight(uint8_t *data, size_t size, uint16_t weightG) {
  if (!data || size <= OpenPrintTagTest::ACTUAL_WEIGHT_VALUE_OFFSET + 1) return;
  data[OpenPrintTagTest::ACTUAL_WEIGHT_VALUE_OFFSET] = uint8_t((weightG >> 8) & 0xFF);
  data[OpenPrintTagTest::ACTUAL_WEIGHT_VALUE_OFFSET + 1] = uint8_t(weightG & 0xFF);
}

/**
 * @brief Печатает короткий hex preview буфера в терминал.
 */
static void rfidPrintBufferPreview(const char *prefix, const uint8_t *data, size_t size) {
  Serial.printf("%s", prefix);
  size_t preview = size < 32 ? size : 32;
  for (size_t i = 0; i < preview; i++) {
    if ((i % 16) == 0) Serial.printf("\n  %03u:", (unsigned)i);
    Serial.printf(" %02X", data[i]);
  }
  Serial.println();
}

/**
 * @brief Записывает тестовый OpenPrintTag-образ на текущую метку и читает его обратно.
 */
static bool rfidRunWriteSelfTest(uint8_t readerId) {
  if (readerId >= RFID_MAX_READERS || !g_rfidTagPresent[readerId]) {
    Serial.println("[RFIDTEST] no active tag, place tag first");
    return false;
  }

  const RfidTag &tag = g_rfidLastTag[readerId];
  const bool isNtag = (tag.type == RfidTagType::NTAG213 ||
                       tag.type == RfidTagType::NTAG215 ||
                       tag.type == RfidTagType::NTAG216);
  const bool isIso15693 = (tag.type == RfidTagType::ISO15693_GENERIC ||
                           tag.type == RfidTagType::ICODE_SLIX2);
  const bool isMifare = (tag.type == RfidTagType::MIFARE_CLASSIC_1K);
  if (!isNtag && !isIso15693 && !isMifare) {
    Serial.printf("[RFIDTEST] unsupported tag type=%u\n", (unsigned)tag.type);
    return false;
  }

  uint8_t writeImage[OpenPrintTagTest::IMAGE_SIZE] = {};
  uint8_t readback[OpenPrintTagTest::IMAGE_SIZE] = {};
  memcpy(writeImage, OpenPrintTagTest::kImage, sizeof(writeImage));

  if (g_rfidTestRuns > 0) {
    g_rfidTestWeightG = (g_rfidTestWeightG > RFID_SELF_TEST_WEIGHT_STEP_G)
                            ? (g_rfidTestWeightG - RFID_SELF_TEST_WEIGHT_STEP_G)
                            : 0;
  }
  rfidTestWriteWeight(writeImage, sizeof(writeImage), g_rfidTestWeightG);
  g_rfidTestRuns++;

  uint8_t startPage;
  size_t writeOffset;
  size_t writeSize;
  if (isNtag) {
    startPage = 4;
    writeOffset = size_t(startPage - OpenPrintTagTest::START_PAGE) * 4;
    writeSize = OpenPrintTagTest::IMAGE_SIZE - writeOffset;
    const size_t capacity = size_t(tag.userBlocks) * size_t(tag.blockSize);
    if (writeSize > capacity) writeSize = capacity;
  } else if (isMifare) {
    startPage = 1; // block 0 = manufacturer, block 3 = trailer → start at 1
    writeOffset = 0;
    const size_t capacity = size_t(tag.userBlocks) * size_t(tag.blockSize);
    writeSize = capacity < OpenPrintTagTest::IMAGE_SIZE ? capacity
                                                        : OpenPrintTagTest::IMAGE_SIZE;
  } else {
    startPage = 0;
    writeOffset = 0;
    const size_t capacity = size_t(tag.userBlocks) * size_t(tag.blockSize);
    writeSize = capacity < OpenPrintTagTest::IMAGE_SIZE ? capacity
                                                        : OpenPrintTagTest::IMAGE_SIZE;
  }

  if (writeSize == 0) {
    Serial.printf("[RFIDTEST] FAIL: writeSize=0 tag has no writable area (userBlocks=%u blockSize=%u)\n",
                  (unsigned)tag.userBlocks,
                  (unsigned)tag.blockSize);
    return false;
  }

  Serial.printf("[RFIDTEST] run=%lu reader=%u unit=%u bytes=%u startPage=%u weight=%u\n",
                (unsigned long)g_rfidTestRuns,
                readerId,
                rfidManager.unitId(readerId),
                (unsigned)writeSize,
                (unsigned)startPage,
                (unsigned)g_rfidTestWeightG);
  if (isNtag) {
    Serial.println("[RFIDTEST] note: NTAG test skips page 3 CC and writes only user pages 4+");
  } else if (isMifare) {
    Serial.println("[RFIDTEST] note: MIFARE test writes user blocks (skips block 0 + trailers)");
  } else {
    Serial.println("[RFIDTEST] note: ISO15693 test writes user blocks from block 0");
  }

  uint32_t writeStartedAt = millis();
  uint32_t writeMaxStepMs = 0;
  bool writeOk = rfidWriteRangeSync(readerId, tag, startPage,
                                    writeImage + writeOffset, writeSize, writeMaxStepMs);
  uint32_t writeMs = millis() - writeStartedAt;
  Serial.printf("[RFIDTEST] write: %s in %lums maxStep=%lums\n",
                writeOk ? "OK" : "FAIL",
                (unsigned long)writeMs,
                (unsigned long)writeMaxStepMs);
  if (!writeOk) return false;

  uint32_t readStartedAt = millis();
  uint32_t readMaxStepMs = 0;
  bool readOk = rfidReadRangeSync(readerId, tag, startPage,
                                  readback + writeOffset, writeSize, readMaxStepMs);
  uint32_t readMs = millis() - readStartedAt;
  Serial.printf("[RFIDTEST] readback: %s in %lums maxStep=%lums\n",
                readOk ? "OK" : "FAIL",
                (unsigned long)readMs,
                (unsigned long)readMaxStepMs);
  if (!readOk) return false;

  uint16_t writtenWeight = rfidTestReadWeight(writeImage, sizeof(writeImage));
  uint16_t readWeight = rfidTestReadWeight(readback, sizeof(readback));
  bool same = (memcmp(writeImage + writeOffset, readback + writeOffset, writeSize) == 0);

  Serial.printf("[RFIDTEST] compare: %s writtenWeight=%u readWeight=%u\n",
                same ? "OK" : "MISMATCH",
                (unsigned)writtenWeight,
                (unsigned)readWeight);
  if (!same) {
    for (size_t i = writeOffset; i < OpenPrintTagTest::IMAGE_SIZE; i++) {
      if (writeImage[i] == readback[i]) continue;
      Serial.printf("[RFIDTEST] first mismatch at offset=%u write=%02X read=%02X\n",
                    (unsigned)i, writeImage[i], readback[i]);
      break;
    }
  }

  rfidPrintBufferPreview("[RFIDTEST] write preview", writeImage, sizeof(writeImage));
  rfidPrintBufferPreview("[RFIDTEST] read  preview", readback, sizeof(readback));
  return same;
}

static bool rfidTagSupportsAutoSelfTest(const RfidTag &tag) {
  return tag.type == RfidTagType::NTAG213 ||
         tag.type == RfidTagType::NTAG215 ||
         tag.type == RfidTagType::NTAG216 ||
         tag.type == RfidTagType::ISO15693_GENERIC ||
         tag.type == RfidTagType::ICODE_SLIX2 ||
         tag.type == RfidTagType::MIFARE_CLASSIC_1K;
}

/**
 * @brief Простейший обработчик debug-команд из USB Serial.
 */
static void serviceSerialDebugCommands() {
  static char cmdBuf[32] = {};
  static uint8_t cmdLen = 0;
  static uint32_t lastRxAtMs = 0;

  // 1. Читаем все доступные байты в буфер.
  while (Serial.available() > 0) {
    char c = static_cast<char>(Serial.read());
    lastRxAtMs = millis();
    if (c == '\r' || c == '\n') continue;  // newline обрабатывается через таймаут
    if (cmdLen + 1 < sizeof(cmdBuf)) {
      cmdBuf[cmdLen++] = c;
    }
  }

  // 2. Коммит по таймауту: PlatformIO Serial Monitor не шлёт \n,
  // поэтому считаем команду завершённой через 100мс тишины.
  if (cmdLen == 0) return;
  if (millis() - lastRxAtMs < 100) return;

  cmdBuf[cmdLen] = '\0';
  while (cmdLen > 0 && (cmdBuf[cmdLen - 1] == ' ' || cmdBuf[cmdLen - 1] == '\t')) {
    cmdBuf[--cmdLen] = '\0';
  }

  if (cmdLen == 0) return;

  Serial.printf("[SERIAL] cmd='%s' len=%u\n", cmdBuf, (unsigned)cmdLen);

  if (strcmp(cmdBuf, "rfidtest") == 0) {
    uint8_t readerId = 0xFF;
    if (rfidFindPresentReader(readerId)) {
      rfidRunWriteSelfTest(readerId);
    } else {
      Serial.println("[RFIDTEST] no active tag, place tag first");
    }
  } else if (strcmp(cmdBuf, "errclear") == 0) {
    errlog_clear();
    error_ack_begin();
    Serial.println("[ERRLOG] cleared");
  } else if (strcmp(cmdBuf, "help") == 0 || strcmp(cmdBuf, "?") == 0) {
    Serial.println("[SERIAL] commands:");
    Serial.println("  errclear");
    Serial.println("  rfidtest");
    Serial.println("  rfidscan <all|a|b|15|f|none>");
    Serial.println("  rfidhelp");
  } else if (strncmp(cmdBuf, "rfidscan ", 9) == 0) {
    const char *arg = cmdBuf + 9;
    RfidProtocolMask mask = 0;
    bool ok = true;
    if (strcmp(arg, "all") == 0) {
      mask = RFID_PROTOCOL_ALL;
    } else if (strcmp(arg, "none") == 0) {
      mask = 0;
    } else if (strcmp(arg, "a") == 0 || strcmp(arg, "ntag") == 0) {
      mask = static_cast<uint8_t>(RfidProtocol::Iso14443A);
    } else if (strcmp(arg, "b") == 0) {
      mask = static_cast<uint8_t>(RfidProtocol::Iso14443B);
    } else if (strcmp(arg, "15") == 0 || strcmp(arg, "slix") == 0) {
      mask = static_cast<uint8_t>(RfidProtocol::Iso15693);
    } else if (strcmp(arg, "f") == 0) {
      mask = static_cast<uint8_t>(RfidProtocol::FeliCa);
    } else {
      Serial.printf("[RFIDTEST] rfidscan: unknown arg '%s' (all|a|b|15|f|none)\n", arg);
      ok = false;
    }
    if (ok) {
      for (uint8_t r = 0; r < rfidManager.readerCount(); r++) {
        if (rfidManager.isAvailable(r)) rfidManager.setScanProtocols(r, mask);
      }
      Serial.printf("[RFIDTEST] scan mask=0x%02X\n", mask);
    }
  } else if (strcmp(cmdBuf, "rfidhelp") == 0) {
    Serial.println("[RFIDTEST] commands: rfidtest, rfidscan <all|a|b|15|f|none>, rfidhelp");
    Serial.println("[SERIAL] use 'help' for all debug commands");
  } else {
    Serial.printf("[RFIDTEST] unknown command: %s\n", cmdBuf);
  }

  cmdLen = 0;
  cmdBuf[0] = '\0';
}

/**
 * @brief Возвращает объём user-memory выбранной метки.
 */
static size_t rfidTagReadableBytes(const RfidTag &tag) {
  if (tag.userBlocks > 0 && tag.blockSize > 0) {
    return static_cast<size_t>(tag.userBlocks) * tag.blockSize;
  }
  switch (tag.type) {
    case RfidTagType::TOPAZ_GENERIC: return 0;
    case RfidTagType::ISO14443A_GENERIC: return 0;
    case RfidTagType::ISO14443B_GENERIC: return 0;
    case RfidTagType::FELICA_GENERIC: return 0;
    case RfidTagType::ISO15693_GENERIC: return 0;
    case RfidTagType::NTAG213: return 144;
    case RfidTagType::NTAG215: return 504;
    case RfidTagType::NTAG216: return 888;
    case RfidTagType::ICODE_SLIX2: return 316;
    default: return 0;
  }
}

/**
 * @brief Возвращает объём verify-прохода для выбранного режима проверки.
 */
static size_t rfidVerifyBytes(size_t writeSize, RfidVerifyMode verifyMode) {
  switch (verifyMode) {
    case RfidVerifyMode::None: return 0;
    case RfidVerifyMode::Header32: return writeSize < 32 ? writeSize : 32;
    case RfidVerifyMode::Full: return writeSize;
  }
  return 0;
}

/**
 * @brief Отправляет собранный буфер метки переменным числом UART-фрагментов
 *        в зависимости от реального размера user-memory метки.
 *
 * Число фрагментов = ceil(job.readSize / RFID_FRAG_SIZE). Для меток меньше
 * 888 Б (NTAG213 144 Б → 1 кадр, SLIX2 316 Б → 2, NTAG215 504 Б → 4,
 * MIFARE 768 Б → 5, NTAG216 888 Б → 6) экономим UART-трафик пропорционально.
 *
 * LINK определяет конец передачи по FLAG_LAST_FRAGMENT на последнем кадре,
 * счётчик фрагментов не проверяет. Хвост LINK-буфера (байты после `readSize`)
 * остаётся нулями с предыдущего memset — backend-контракт 888 Б сохранён.
 *
 * Последний фрагмент сам по себе всегда полон на 163 Б (структура фикс-размера);
 * реальная длина данных — `job.readSize - srcOff`, остаток внутри `dp.fragment`
 * остаётся нулями из `dp{}`-инициализации.
 */
static void rfidFlushReadJob(RfidReadJob &job) {
  // UID → HEX-строка для поля tag (валидация на ESP32)
  char tagHex[32] = {};
  int off = 0;
  for (uint8_t b = 0; b < job.tag.uidLen && off < 31; b++)
    off += snprintf(tagHex + off, sizeof(tagHex) - off,
                    b ? ":%02X" : "%02X", job.tag.uid[b]);

  // Защита от readSize=0 (не должно случаться, но не шлём совсем ничего —
  // ответим одним пустым фрагментом с FLAG_LAST_FRAGMENT).
  const size_t readSize = job.readSize == 0 ? 1 : job.readSize;
  const uint8_t nFrags = static_cast<uint8_t>(
      (readSize + RFID_FRAG_SIZE - 1) / RFID_FRAG_SIZE);

  for (uint8_t f = 0; f < nFrags; f++) {
    idryer::UartRfidDataPayload dp{};
    dp.readerId = job.readerId;
    dp.unitId   = job.unitId;
    memcpy(dp.tag, tagHex, sizeof(dp.tag));

    size_t srcOff  = (size_t)f * RFID_FRAG_SIZE;
    size_t copyLen = readSize > srcOff ? readSize - srcOff : 0;
    if (copyLen > RFID_FRAG_SIZE) copyLen = RFID_FRAG_SIZE;
    if (copyLen > 0) memcpy(dp.fragment, job.data + srcOff, copyLen);

    bool isLast  = (f == nFrags - 1);
    uint8_t flags = isLast ? idryer::UART_FLAG_LAST_FRAGMENT : idryer::UART_FLAG_FRAGMENT;
    uartBridge.sendRfidReadData(dp, flags);
  }
  DEBUG_I("[RFID] rfidSendTagData: sent %u frags reader=%u bytes=%u in %lums",
          nFrags, job.readerId, (unsigned)job.readSize,
          (unsigned long)(millis() - job.startedAtMs));
  DEBUG_I("[RFID] rfidSendTagData: maxStep reader=%u step=%lums",
          job.readerId, (unsigned long)job.maxStepMs);
}

/**
 * @brief Выполняет синхронное полное чтение содержимого метки.
 */
static bool rfidReadTagDataSync(uint8_t readerId) {
  if (readerId >= RFID_MAX_READERS || !g_rfidTagPresent[readerId]) return false;

  RfidReadJob job{};
  job.active = true;
  job.readerId = readerId;
  job.unitId = rfidManager.unitId(readerId);
  job.tag = g_rfidLastTag[readerId];
  if (!rfidTagBlockSizeSupported(job.tag)) return false;
  const uint8_t blockSize = rfidTagBlockSize(job.tag);
  job.readSize = rfidTagReadableBytes(job.tag);
  job.startedAtMs = millis();
  const uint8_t startPage = rfidTagStartPage(job.tag);

  for (size_t offset = 0; offset < job.readSize; offset += blockSize) {
    uint32_t stepStartedAt = millis();
    uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
    uint8_t pageNo = rfidTagPageNoAt(job.tag, offset / blockSize);
    RfidStatus st = rfidManager.read(readerId, job.tag, pageNo, block, blockSize);
    uint32_t stepMs = millis() - stepStartedAt;
    if (stepMs > job.maxStepMs) job.maxStepMs = stepMs;
    if (st != RfidStatus::OK) {
      DEBUG_W("[RFID] sync read failed reader=%u page=%u st=%u",
              readerId, pageNo, (uint8_t)st);
      return false;
    }
    memcpy(job.data + offset, block, blockSize);
  }

  rfidFlushReadJob(job);
  return true;
}

/**
 * @brief Запускает фоновое чтение пользовательской памяти текущей метки.
 */
static void rfidStartSendTagData(uint8_t readerId) {
  if (readerId >= RFID_MAX_READERS) return;
  if (!g_rfidTagPresent[readerId]) {
    DEBUG_W("[RFID] rfidSendTagData: no tag on reader=%u", readerId);
    return;
  }

  RfidReadJob &job = g_rfidReadJobs[readerId];
  job.active = true;
  job.readerId = readerId;
  job.unitId = rfidManager.unitId(readerId);
  job.tag = g_rfidLastTag[readerId];
  if (!rfidTagBlockSizeSupported(job.tag)) {
    DEBUG_W("[RFID] rfidSendTagData: unsupported block size reader=%u size=%u",
            readerId, (unsigned)rfidTagBlockSize(job.tag));
    job.active = false;
    return;
  }
  job.readSize = rfidTagReadableBytes(job.tag);
  job.nextOffset = 0;
  job.startedAtMs = millis();
  job.maxStepMs = 0;
  memset(job.data, 0, sizeof(job.data));
  DEBUG_I("[RFID] rfidSendTagData: scheduled reader=%u bytes=%u", readerId, (unsigned)job.readSize);
}

/**
 * @brief Запускает чтение содержимого метки в выбранном режиме.
 */
static bool rfidRequestReadTagData(uint8_t readerId, RfidTransferMode mode) {
  if (mode == RfidTransferMode::Sync) {
    return rfidReadTagDataSync(readerId);
  }
  rfidStartSendTagData(readerId);
  return true;
}

/**
 * @brief Выполняет фоновое чтение RFID-памяти малыми шагами.
 */
static void rfidServiceReadJobs() {
  for (uint8_t readerId = 0; readerId < RFID_MAX_READERS; readerId++) {
    RfidReadJob &job = g_rfidReadJobs[readerId];
    if (!job.active) continue;

    if (!g_rfidTagPresent[readerId] || !rfidUidEqual(job.tag, g_rfidLastTag[readerId])) {
      job.active = false;
      DEBUG_W("[RFID] rfidSendTagData: aborted reader=%u tag changed/removed", readerId);
      continue;
    }

    const uint8_t blockSize = rfidTagBlockSize(job.tag);
    if (!rfidTagBlockSizeSupported(job.tag)) {
      job.active = false;
      DEBUG_W("[RFID] rfidSendTagData: unsupported block size reader=%u size=%u",
              readerId, (unsigned)blockSize);
      continue;
    }

    for (uint8_t pageStep = 0; pageStep < RFID_BLOCKS_PER_STEP; pageStep++) {
      if (job.nextOffset >= job.readSize) {
        rfidFlushReadJob(job);
        job.active = false;
        break;
      }

      uint32_t stepStartedAt = millis();
      uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
      uint8_t pageNo = rfidTagPageNoAt(job.tag, job.nextOffset / blockSize);
      RfidStatus st = rfidManager.read(readerId, job.tag, pageNo, block, blockSize);
      uint32_t stepMs = millis() - stepStartedAt;
      if (stepMs > job.maxStepMs) job.maxStepMs = stepMs;
      if (st != RfidStatus::OK) {
        DEBUG_W("[RFID] rfidSendTagData: read failed reader=%u page=%u st=%u",
                readerId, pageNo, (uint8_t)st);
        job.active = false;
        break;
      }

      memcpy(job.data + job.nextOffset, block, blockSize);
      job.nextOffset += blockSize;
    }
  }
}

/**
 * @brief Выполняет синхронную запись пользовательских данных на метку.
 */
static bool rfidWriteTagDataSync(uint8_t readerId, const uint8_t *data, size_t writeSize,
                                 RfidVerifyMode verifyMode) {
  if (readerId >= RFID_MAX_READERS || !g_rfidTagPresent[readerId] || !data || writeSize > RFID_DATA_SIZE) {
    return false;
  }

  const RfidTag tag = g_rfidLastTag[readerId];
  if (!rfidTagBlockSizeSupported(tag)) return false;
  const uint8_t blockSize = rfidTagBlockSize(tag);
  if (writeSize > rfidTagReadableBytes(tag)) return false;
  uint32_t startedAtMs = millis();
  uint32_t maxStepMs = 0;
  const uint8_t startPage = rfidTagStartPage(tag);

  for (size_t offset = 0; offset < writeSize; offset += blockSize) {
    uint32_t stepStartedAt = millis();
    uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
    size_t copyLen = writeSize - offset;
    if (copyLen > blockSize) copyLen = blockSize;
    memcpy(block, data + offset, copyLen);

    uint8_t pageNo = rfidTagWritePageNoAt(tag, offset / blockSize);
    RfidStatus st = rfidManager.write(readerId, tag, pageNo, block, blockSize);
    uint32_t stepMs = millis() - stepStartedAt;
    if (stepMs > maxStepMs) maxStepMs = stepMs;
    if (st != RfidStatus::OK) {
      DEBUG_W("[RFID] sync write failed reader=%u page=%u st=%u",
              readerId, pageNo, (uint8_t)st);
      return false;
    }
  }

  size_t verifySize = rfidVerifyBytes(writeSize, verifyMode);
  for (size_t offset = 0; offset < verifySize; offset += blockSize) {
    uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
    uint8_t pageNo = rfidTagWritePageNoAt(tag, offset / blockSize);
    RfidStatus st = rfidManager.read(readerId, tag, pageNo, block, blockSize);
    if (st != RfidStatus::OK) {
      DEBUG_W("[RFID] sync verify read failed reader=%u page=%u st=%u",
              readerId, pageNo, (uint8_t)st);
      return false;
    }
    size_t compareLen = verifySize - offset;
    if (compareLen > blockSize) compareLen = blockSize;
    if (memcmp(block, data + offset, compareLen) != 0) {
      DEBUG_W("[RFID] sync verify mismatch reader=%u page=%u", readerId, pageNo);
      return false;
    }
  }

  DEBUG_I("[RFID] sync write done reader=%u bytes=%u verify=%u in %lums maxStep=%lums",
          readerId, (unsigned)writeSize, (unsigned)verifySize,
          (unsigned long)(millis() - startedAtMs), (unsigned long)maxStepMs);
  return true;
}

/**
 * @brief Запускает фоновую запись пользовательских данных на метку.
 */
static bool rfidStartWriteTagData(uint8_t readerId, const uint8_t *data, size_t writeSize,
                                  RfidVerifyMode verifyMode) {
  if (readerId >= RFID_MAX_READERS || !g_rfidTagPresent[readerId] || !data || writeSize > RFID_DATA_SIZE) {
    return false;
  }

  RfidWriteJob &job = g_rfidWriteJobs[readerId];
  job.active = true;
  job.verifyPhase = false;
  job.readerId = readerId;
  job.unitId = rfidManager.unitId(readerId);
  job.tag = g_rfidLastTag[readerId];
  if (!rfidTagBlockSizeSupported(job.tag)) return false;
  if (writeSize > rfidTagReadableBytes(job.tag)) return false;
  job.writeSize = writeSize;
  job.nextOffset = 0;
  job.startedAtMs = millis();
  job.maxStepMs = 0;
  job.verifyMode = verifyMode;
  memset(job.data, 0, sizeof(job.data));
  memcpy(job.data, data, writeSize);
  DEBUG_I("[RFID] async write scheduled reader=%u bytes=%u verify=%u",
          readerId, (unsigned)writeSize, (unsigned)rfidVerifyBytes(writeSize, verifyMode));
  return true;
}

/**
 * @brief Запускает запись пользовательских данных в выбранном режиме.
 */
static bool rfidRequestWriteTagData(uint8_t readerId, const uint8_t *data, size_t writeSize,
                                    RfidTransferMode mode, RfidVerifyMode verifyMode) {
  if (mode == RfidTransferMode::Sync) {
    return rfidWriteTagDataSync(readerId, data, writeSize, verifyMode);
  }
  return rfidStartWriteTagData(readerId, data, writeSize, verifyMode);
}

/**
 * @brief Обработчик UART-кадров RfidData (0x1A/0x1B).
 *
 * MCU получает только RfidWriteData (0x1B) — фрагменты данных для записи на метку.
 * RfidReadData (0x1A) MCU сам отправляет, не принимает.
 *
 * Фрагменты складываются в staging-буфер, заранее подготовленный командой
 * WriteRfid (0x08, arg0 = ожидаемый размер). По FLAG_LAST_FRAGMENT запускается
 * реальная запись через rfidStartWriteTagData().
 */
static void handleUartRfidData(const idryer::UartRfidDataPayload &payload,
                               const idryer::UartFrameHeader &header) {
  // Lambda для ACK-ответа на фрагмент (stop-and-wait flow control).
  const bool ackRequested = (header.flags & idryer::UART_FLAG_ACK_REQ) != 0;
  auto sendAckIfRequested = [&](idryer::UartErrCode status) {
    if (ackRequested) {
      uartBridge.sendCommandAck(header.sequence, status);
    }
  };

  // MCU принимает только WriteData (0x1B). ReadData (0x1A) — игнорируем (безопасность).
  if (header.kind != idryer::UartMsgKind::RfidWriteData) {
    DEBUG_W("[RFID] unexpected RfidData kind=0x%02X (ignored)",
            static_cast<unsigned>(header.kind));
    sendAckIfRequested(idryer::UartErrCode::InvalidPayload);
    return;
  }

  // LINK знает только unitId, readerId в payload может быть 0xFF.
  // Ищем ридер по unitId — так же, как в rfidArmWriteStaging().
  uint8_t unit = payload.unitId;
  uint8_t r = 0xFF;
  for (uint8_t i = 0; i < rfidManager.readerCount(); i++) {
    if (rfidManager.isAvailable(i) && rfidManager.unitId(i) == unit) {
      r = i;
      break;
    }
  }
  if (r == 0xFF) {
    DEBUG_W("[RFID] write fragment: no reader for unit=%u (ignored)", unit);
    sendAckIfRequested(idryer::UartErrCode::InvalidPayload);
    return;
  }

  RfidWriteStaging &s = g_rfidWriteStaging[r];
  if (!s.active) {
    DEBUG_W("[RFID] write fragment ignored: staging not armed reader=%u", r);
    sendAckIfRequested(idryer::UartErrCode::InvalidPayload);
    return;
  }

  // Таймаут на сборку — если между командой WriteRfid и фрагментами прошло слишком много.
  if (millis() - s.armedAtMs > RFID_WRITE_STAGING_TIMEOUT_MS) {
    DEBUG_W("[RFID] write staging expired reader=%u age=%lums", r,
            (unsigned long)(millis() - s.armedAtMs));
    s.active = false;
    sendAckIfRequested(idryer::UartErrCode::Timeout);
    return;
  }

  bool isLast = (header.flags & idryer::UART_FLAG_LAST_FRAGMENT) != 0;
  size_t remaining = (s.expectedSize > s.receivedSize) ? (s.expectedSize - s.receivedSize) : 0;
  size_t copyLen = RFID_FRAG_SIZE < remaining ? RFID_FRAG_SIZE : remaining;
  if (copyLen > 0) {
    memcpy(s.data + s.receivedSize, payload.fragment, copyLen);
    s.receivedSize += copyLen;
  }

  DEBUG_I("[RFID] write fragment reader=%u received=%u/%u last=%u",
          r, (unsigned)s.receivedSize, (unsigned)s.expectedSize, isLast ? 1 : 0);

  if (isLast) {
    if (s.receivedSize < s.expectedSize) {
      DEBUG_W("[RFID] write staging short: got=%u expected=%u reader=%u",
              (unsigned)s.receivedSize, (unsigned)s.expectedSize, r);
      s.active = false;
      sendAckIfRequested(idryer::UartErrCode::InvalidPayload);
      return;
    }
    bool ok = rfidStartWriteTagData(r, s.data, s.expectedSize, s.verifyMode);
    DEBUG_I("[RFID] write staging complete reader=%u bytes=%u job=%s",
            r, (unsigned)s.receivedSize, ok ? "STARTED" : "REJECTED");
    s.active = false;
    sendAckIfRequested(ok ? idryer::UartErrCode::None : idryer::UartErrCode::InvalidPayload);
    return;
  }

  // Promiscuous fragment (не last) — accepted, ACK None.
  sendAckIfRequested(idryer::UartErrCode::None);
}

/**
 * @brief Готовит staging-буфер для приёма фрагментов RfidWriteData.
 * Вызывается при обработке команды WriteRfid (0x08).
 * @return true если staging подготовлен, false если ридер/метка не готовы.
 */
static bool rfidArmWriteStaging(uint8_t readerId, size_t expectedSize,
                                RfidVerifyMode verifyMode) {
  if (readerId >= RFID_MAX_READERS || !rfidManager.isAvailable(readerId)) return false;
  if (expectedSize == 0 || expectedSize > RFID_DATA_SIZE) return false;
  if (!g_rfidTagPresent[readerId]) return false;

  RfidWriteStaging &s = g_rfidWriteStaging[readerId];
  s.active = true;
  s.readerId = readerId;
  s.unitId = rfidManager.unitId(readerId);
  s.verifyMode = verifyMode;
  s.expectedSize = expectedSize;
  s.receivedSize = 0;
  s.armedAtMs = millis();
  memset(s.data, 0, sizeof(s.data));
  // Sync-флаг больше не нужен — staging.active взял эстафету в poll-guard.
  g_rfidWritePending[readerId] = false;
  return true;
}

/**
 * @brief Синхронно армирует staging для приёма WriteRfid фрагментов по unitId.
 * Вызывается из uart_manager.cpp при CommandCode::WriteRfid в контексте uartBridge.tick().
 * ВАЖНО: armed должен произойти до того, как парсер UART начнёт разбирать фрагменты из
 * того же буфера (LINK шлёт Command + frag1 + frag2 без пауз). Enqueue в main loop
 * создаёт окно, в котором frag1 отбрасывается как "staging not armed".
 */
bool rfidArmWriteStagingByUnit(uint8_t unitId, uint32_t expectedSize, uint32_t verifyCode) {
  uint8_t readerId = 0xFF;
  for (uint8_t r = 0; r < rfidManager.readerCount(); r++) {
    if (rfidManager.isAvailable(r) && rfidManager.unitId(r) == unitId) {
      readerId = r;
      break;
    }
  }
  if (readerId == 0xFF) return false;

  RfidVerifyMode verify = (verifyCode == 0) ? RfidVerifyMode::None
                          : (verifyCode == 2) ? RfidVerifyMode::Full
                                              : RfidVerifyMode::Header32;
  // Флаг pending ставим до arm'а — на случай если armed не пройдёт
  // (например, метки нет), всё равно блокируем poll на таймаут, чтобы
  // не шуметь в логе во время приёма уже идущих фрагментов.
  g_rfidWritePending[readerId] = true;
  g_rfidWritePendingUntilMs[readerId] = millis() + RFID_WRITE_STAGING_TIMEOUT_MS;
  return rfidArmWriteStaging(readerId, expectedSize, verify);
}

/**
 * @brief Выполняет фоновые шаги записи/verify для метки.
 */
static void rfidServiceWriteJobs() {
  for (uint8_t readerId = 0; readerId < RFID_MAX_READERS; readerId++) {
    RfidWriteJob &job = g_rfidWriteJobs[readerId];
    if (!job.active) continue;

    if (!g_rfidTagPresent[readerId] || !rfidUidEqual(job.tag, g_rfidLastTag[readerId])) {
      job.active = false;
      DEBUG_W("[RFID] async write aborted reader=%u tag changed/removed", readerId);
      continue;
    }

    const uint8_t blockSize = rfidTagBlockSize(job.tag);
    if (!rfidTagBlockSizeSupported(job.tag)) {
      job.active = false;
      DEBUG_W("[RFID] async write aborted reader=%u unsupported block size=%u",
              readerId, (unsigned)blockSize);
      continue;
    }

    for (uint8_t pageStep = 0; pageStep < RFID_BLOCKS_PER_STEP; pageStep++) {
      size_t phaseSize = job.verifyPhase ? rfidVerifyBytes(job.writeSize, job.verifyMode) : job.writeSize;
      if (job.nextOffset >= phaseSize) {
        if (!job.verifyPhase && job.verifyMode != RfidVerifyMode::None) {
          job.verifyPhase = true;
          job.nextOffset = 0;
          continue;
        }

        DEBUG_I("[RFID] async write done reader=%u bytes=%u verify=%u in %lums maxStep=%lums",
                readerId, (unsigned)job.writeSize,
                (unsigned)rfidVerifyBytes(job.writeSize, job.verifyMode),
                (unsigned long)(millis() - job.startedAtMs),
                (unsigned long)job.maxStepMs);
        job.active = false;
        break;
      }

      uint32_t stepStartedAt = millis();
      uint8_t block[RFID_MAX_BLOCK_SIZE] = {};
      uint8_t pageNo = rfidTagWritePageNoAt(job.tag, job.nextOffset / blockSize);
      size_t copyLen = phaseSize - job.nextOffset;
      if (copyLen > blockSize) copyLen = blockSize;

      if (!job.verifyPhase) {
        memcpy(block, job.data + job.nextOffset, copyLen);
        RfidStatus st = rfidManager.write(readerId, job.tag, pageNo, block, blockSize);
        uint32_t stepMs = millis() - stepStartedAt;
        if (stepMs > job.maxStepMs) job.maxStepMs = stepMs;
        if (st != RfidStatus::OK) {
          DEBUG_W("[RFID] async write failed reader=%u page=%u st=%u",
                  readerId, pageNo, (uint8_t)st);
          job.active = false;
          break;
        }
      } else {
        RfidStatus st = rfidManager.read(readerId, job.tag, pageNo, block, blockSize);
        uint32_t stepMs = millis() - stepStartedAt;
        if (stepMs > job.maxStepMs) job.maxStepMs = stepMs;
        if (st != RfidStatus::OK) {
          DEBUG_W("[RFID] async verify read failed reader=%u page=%u st=%u",
                  readerId, pageNo, (uint8_t)st);
          job.active = false;
          break;
        }
        if (memcmp(block, job.data + job.nextOffset, copyLen) != 0) {
          DEBUG_W("[RFID] async verify mismatch reader=%u page=%u", readerId, pageNo);
          job.active = false;
          break;
        }
      }

      job.nextOffset += blockSize;
    }
  }
}
// -----------------------------------------------------------------------
// Обработка оверлея ошибок. Возвращает true если ошибки активны (и нужно прервать loop)
bool handleErrorOverlay() {
  if (!error_ack_active()) return false; // Нет ошибок - продолжаем нормальную работу

  uint32_t now = millis();

  if (hasLink()) {
    uartBridge.loop();
    handlePendingUartCommands();
    errorReportTick(now);
  }

  handleAnalogButtons(now);

  int8_t delta = 0;
  bool click = false;
  bool hold = false;
  if (hasScreen()) {
    adc_btns.tick();
    delta = enc->tick();
    ButtonEvent btn2_event = adc_btns.getEvent(2);
    hold = btn2_event == ButtonEvent::LongHold;
    if (hold) DEBUG_C("Errors cleared via button hold");
  }

  ledsTick(now);
  error_ack_update(delta, click, hold, now);

  // Проверяем, вышли ли из режима ошибок
  if (!error_ack_active()) {
    digitalWrite(FAN0, 0);
    digitalWrite(FAN1, 0);
    digitalWrite(FAN2, 0);
    DEBUG_I("Error acknowledgment complete, resuming normal operation");
  }

  delay(5); // Небольшая задержка, чтобы не перегружать цикл

  return true; // Ошибки активны - прерываем loop
}

void printDryerInputs(const char *name, const DryerInputs &in, uint32_t now) {
  static uint32_t last_print = 0;
  if (now - last_print < 2500) return; // раз в 2 секунды
  last_print = now;
  DEBUG_I("AirT:%.2f, Hum:%.2f, HeaterT:%.2f", in.airTempC, in.airHumRH, in.heaterTempC);
}

static const char *modeToStr(DryerMode m) {
  switch (m) {
  case DryerMode::Idle:
    return "Idle";
  case DryerMode::Drying:
    return "Drying";
  case DryerMode::Storage:
    return "Storage";
  case DryerMode::PidAutoTune:
    return "AutoTune";
  default:
    return "?";
  }
}

static void printControllerBrief(uint32_t now, DryerMode m) {
  static uint32_t last = 0;
  if (now - last < 2000) return;
  last = now;
  DEBUG_I("mode=%s air_sp=", modeToStr(m));
}

void safety_supervisor_tick(uint32_t now);

// Отправка сохранённых ошибок в бэк по UART
static void sendStoredErrorsToBackend() {
  if (!hasLink()) return;
  uint16_t count = errlog_count();
  if (count == 0) return;

  for (uint16_t i = 0; i < count; ++i) {
    ErrLogRec rec{};
    if (!errlog_read_ith_oldest(i, &rec)) break;

    idryer::UartLogPayload payload{};
    const char *sev = (rec.severity == ERRSEV_CRITICAL) ? "CRIT" : (rec.severity == ERRSEV_ERROR) ? "ERROR" : (rec.severity == ERRSEV_WARNING) ? "WARN" : "INFO";
    strncpy(payload.severity, sev, sizeof(payload.severity) - 1);

    const char *src = errsrc_short_name((ErrSource)rec.source);
    if (src) strncpy(payload.source, src, sizeof(payload.source) - 1);

    const char *evt = errcode_machine_name((ErrCode)rec.code);
    if (evt) strncpy(payload.event, evt, sizeof(payload.event) - 1);

    const char *msg = errcode_human_text((ErrCode)rec.code);
    if (msg) strncpy(payload.message, msg, sizeof(payload.message) - 1);

    payload.unitId = rec.ctrl_id;

    uartBridge.sendLog(payload);
  }
}

static void printStoredErrorsToSerial() {
  uint16_t count = errlog_count();
  if (count == 0) return;

  Serial.printf("[ERRLOG] stored errors: count=%u\n", (unsigned)count);

  for (uint16_t i = 0; i < count; ++i) {
    ErrLogRec rec{};
    if (!errlog_read_ith_oldest(i, &rec)) break;

    ErrorEvent ev{};
    ev.ts_ms = rec.ts_ms;
    ev.severity = (ErrSeverity)rec.severity;
    ev.source = (ErrSource)rec.source;
    ev.code = (ErrCode)rec.code;
    ev.msg = errcode_human(ev.code);
    ev.data = rec.data;
    ev.ctrl_id = rec.ctrl_id;

    char line[160];
    error_format_line(&ev, line, sizeof(line));
    Serial.printf("[ERRLOG] #%u count=%u %s\n", (unsigned)i, (unsigned)rec.count, line);
  }
}

// Периодическая отправка ошибок, пока лог не очищен (работает из serviceTasks)
static void errorReportTick(uint32_t now) {
  static uint32_t next_send_ms = 0;
  static uint32_t next_serial_ms = 0;
  static bool led_error_active = false;

  bool hasErrors = errlog_count() > 0;

  if (hasErrors) {
    // Красное дыхание, пока ошибки не сброшены
    if (!led_error_active) {
      ledsShowAlertBreath(LedColors::RED, 1200, 1000 * 60 * 60 * 24); // 24 часа (постоянно)
      led_error_active = true;
      printStoredErrorsToSerial();
      next_serial_ms = now + 15000;
    }

    if ((int32_t)(now - next_serial_ms) >= 0) {
      printStoredErrorsToSerial();
      next_serial_ms = now + 15000;
    }

    // Ждём, пока LINK станет готовым, чтобы не слать в пустоту
    if (uartLinkReady() && (int32_t)(now - next_send_ms) >= 0) {
      sendStoredErrorsToBackend();
      next_send_ms = now + 15000; // каждые ~15 секунд
    }
  } else {
    if (led_error_active) {
      // Успешный сброс ошибок — зелёное дыхание на 3 секунды
      ledsShowAlertBreath(LedColors::GREEN, 1200, 3000);
    }
    led_error_active = false;
    next_send_ms = now + 15000;
    next_serial_ms = now + 15000;
  }
}

// ============================================================================
// UART Protocol - см. src/uart/uart_manager.h
// ============================================================================

// void pump_errors_to_serial() {
//   ErrorEvent ev; char line[160];
//   while (errorbus_poll(&ev)) {
//     errlog_append_from_event(&ev);
//     error_format_line(&ev, line, sizeof(line));
//     DEBUG_E("%s\n", line);
//     error_ack_begin();
//   }
// }


//****************** RFID ***************/

//****************** RFID ***************/

void setup() {
  Serial.begin(115200); // Initialize serial communication at 115200 baud rate
  waitSerial();
  // [TEMP-DEBUG] окно 8 секунд чтобы успеть подключиться serial и поймать BOOT-CHECK
  uint32_t bootDelayEnd = millis() + 8000;
  while (millis() < bootDelayEnd) {
    Serial.printf("[BOOT-WAIT] t=%lu\n", millis());
    delay(500);
  }
  DEBUG_I("start");
  Serial.printf("\n\niDryerRP2040 %s\n", VERSION_STR);
  // delay(200);
  gscr_next_unit_rotate_ms = millis() + UNIT_ROTATE_MS;

  errorbus_init();

  // ----------------- SHT31 & SCREEN -----------------
  Wire.setSDA(SDA12);
  Wire.setSCL(SCL12);
  // Wire.setClock(100000);
  Wire.begin();

  // Wire1 только если используется экран (Port3 == SCREEN)
  // Инициализация Wire1 ДО loadFromEEPROM, но проверка port3_mode ПОСЛЕ
  // Поэтому откладываем Wire1.begin() до проверки hasScreen()

  // ----------------- EEPROM -----------------
  EEPROM.begin(EE_EEPROM_SIZE);
  // sys_shadow_init();
  menu.initDefaults();
  menu.loadFromEEPROM();
  // Валидация загруженных значений из EEPROM
  bool needSave = false;

  if (menu.units_count < 1 || menu.units_count > 3) {
    DEBUG_W("Invalid units_count=%u in EEPROM, resetting to 1", menu.units_count);
    menu.units_count = 1;
    needSave = true;
  }
  if (menu.language > 1) {
    DEBUG_W("Invalid language=%u in EEPROM, resetting to 1", menu.language);
    menu.language = 1;
    needSave = true;
  }

  // Валидация температурных пределов (критично для безопасности!)
  for (uint8_t i = 0; i < NUM_UNITS; i++) {
    if (menu.air_max_temp[i] < 50.0f || menu.air_max_temp[i] > 150.0f) {
      DEBUG_W("Invalid air_max_temp[%u]=%.1f in EEPROM, resetting to 90.0", i, menu.air_max_temp[i]);
      menu.air_max_temp[i] = 90.0f;
      needSave = true;
    }
    if (menu.heater_max_temp[i] < 80.0f || menu.heater_max_temp[i] > 200.0f) {
      DEBUG_W("Invalid heater_max_temp[%u]=%.1f in EEPROM, resetting to 130.0", i, menu.heater_max_temp[i]);
      menu.heater_max_temp[i] = 130.0f;
      needSave = true;
    }
  }

  uint32_t magic = 0, ver = 0;
  ee_read(EE_SYS_OFF_MAGIC, magic);
  ee_read(EE_SYS_OFF_VERSION, ver);
  const bool layoutValid = (magic == EE_MAGIC && ver == EE_VERSION);
  if (sessionCountersRestoreFromBackup(menu, layoutValid)) {
    DEBUG_W("Session counters restored from backup");
  }
  if (!layoutValid) needSave = true;

  if (needSave) {
    DEBUG_W("Corrupted EEPROM detected, saving corrected values");
    menu.saveToEEPROM();
    DEBUG_W("Corrupted EEPROM corrected and saved");
  }

  Serial.printf("[BOOT-CHECK] after-validate menu.uc=%u addr=%p needSave=%d\n",
                menu.units_count, (void*)&menu.units_count, (int)needSave);

  // Фиксируем конфигурацию портов на весь сеанс.
  // menu.port*_mode может свободно редактироваться и сохраняться в EEPROM,
  // но железо работает по снапшоту до перезагрузки.
  snapshotPortConfig();

  errlog_init_eeprom();
  print_all_settings(Serial);
  // errlog_dump_to_serial();

  // ----------------- WorkTimeCounter -----------------
  wtc.beginNoInit(); // EEPROM уже начат выше
  wtc.start();       // Запускаем подсчёт времени
  // delay(200);

  // ----------------- Wire1 I2C (GPIO22/23) -----------------
  // Wire1 нужен для SHT31 сенсоров (airSensor1, airSensor2) при units_count >= 2,
  // а также для экрана. Инициализируем всегда когда есть потребители.
  if (hasScreen() || menu.units_count >= 2) {
    Wire1.setSDA(SDA13);
    Wire1.setSCL(SCL13);
    Wire1.begin();
    DEBUG_I("Wire1 initialized (hasScreen=%d, units=%u)", hasScreen(), menu.units_count);
  }

  // ----------------- CRADLE AUTO-DETECT -----------------
  // Инициализируем UartBridge на кроватке (GPIO16/17) и отправляем HelloRequest.
  // Link ответит HelloAck независимо от своего состояния (новый или уже работал).
  // На старых ревизиях плат GPIO16/17 свободны — вреда нет.
  DEBUG_I("=== Initializing Serial1 for cradle detection ===");
  Serial1.setTX(CRADLE_TX_PIN);
  Serial1.setRX(CRADLE_RX_PIN);
  Serial1.setFIFOSize(4096);  // 4КБ: OTA-proxy устойчивость к паузам loop (см. serialPort3)
  DEBUG_I("Serial1 configured: TX=GPIO%d, RX=GPIO%d, FIFO=4096, BAUD=%d", CRADLE_TX_PIN, CRADLE_RX_PIN, UART_BAUD_RATE);

  initArduinoHal(&Serial); // нужен до initUartBridge
  if (g_hal && g_hal->logger) g_hal->logger->setLevel(idryer::hal::LogLevel::Warning);
  initUartBridge(&uartSerial1, UART_BAUD_RATE);
  // Handler для RfidWriteData (0x1B) регистрируется здесь — в uart_manager
  // нет доступа к RFID-staging буферам (живут в main.cpp).
  uartBridge.setRfidDataHandler(handleUartRfidData);
  DEBUG_I("UartBridge initialized on Serial1");

  if (hasScreen()) {
    // Создаем энкодер и кнопку только для экрана (захватывают GPIO 0,1,2)
    enc = new ISRencoder(ENCODER_S1, ENCODER_S2, INPUT_PULLUP, CHANGE);
    enc_btn = new ISRbutton(PSH, INPUT_PULLUP, CHANGE, LOW, 600, 9000);
    DEBUG_I("Encoder and button initialized");

    u8g2.begin();
    u8g2.setFlipMode(0);
    u8g2.getU8x8()->x_offset = 0;
    ui.begin(&u8g2, &menu.language);
    ui.setSaveCallback([]() { menu.saveToEEPROM(); });
    // ----------------- WELCOME SCREEN -----------------
    u8g2.clearBuffer();
    u8g2.enableUTF8Print();
    u8g2.setFontRefHeightExtendedText();
    u8g2.setDrawColor(1);
    u8g2.setFontPosTop();
    u8g2.setFontDirection(0);
    uint8_t scr_w = u8g2.getDisplayWidth();
    uint8_t scr_h = u8g2.getDisplayHeight();
    const char *line1 = "iDryerRP2040";
    const char *line2 = "mr R.";
    const char *line3 = VERSION_STR;
    char line4[32];
    snprintf(line4, sizeof(line4), "WT: %uh %um", wtc.getHours(), wtc.getMinutes());
    uint8_t h = u8g2.getMaxCharHeight();
    int total_h = 4 * h;
    int start_y = (scr_h - total_h) / 2;
    u8g2.drawUTF8((scr_w - u8g2.getStrWidth(line1)) / 2, start_y, line1);
    u8g2.drawUTF8((scr_w - u8g2.getStrWidth(line2)) / 2, start_y + h, line2);
    u8g2.drawUTF8((scr_w - u8g2.getStrWidth(line3)) / 2, start_y + 2 * h, line3);
    u8g2.drawUTF8((scr_w - u8g2.getStrWidth(line4)) / 2, start_y + 3 * h, line4);
    u8g2.sendBuffer();

    uint32_t welcomeStartTime = millis();
    const uint32_t minWelcomeTime = 3000; // Минимум 3 секунды на заставку

    // Ждём до 10000ms, отправляя Hello Request каждую секунду
    {
      uint32_t deadline = millis() + 10000;
      uint32_t startTime = millis();
      uint32_t lastLog = startTime;
      uint32_t lastProbe = 0;
      uint32_t probeInterval = 1000; // Отправляем probe каждую секунду
      uint32_t checks = 0;
      uint32_t probeAttempts = 0;

      DEBUG_I("=== Cradle Port0 detection started (timeout=10000ms, probe interval=1000ms) ===");

      while (millis() < deadline) {
        checks++;
        uint32_t now = millis();

        // Отправляем Hello Request probe каждые probeInterval ms
        if (now - lastProbe >= probeInterval) {
          idryer::UartHelloPayload probe = buildHelloPayload(menu, wtc);
          uartBridge.sendHello(probe, false);
          probeAttempts++;
          lastProbe = now;
          uint32_t elapsed = now - startTime;
          DEBUG_I("  Probe #%lu sent at t=%dms", probeAttempts, (int)elapsed);
        }

        // Проверяем ответ
        if (Serial1.available() > 0) {
          uartSerialPtr = &uartSerial1;
          uint32_t elapsed = millis() - startTime;
          DEBUG_I("✓ Cradle: Link detected on GPIO%d/GPIO%d (Port0, t=%dms, checks=%lu, probes=%lu)", CRADLE_TX_PIN, CRADLE_RX_PIN, (int)elapsed, checks, probeAttempts);
          break;
        }

        // Логируем прогресс каждые 2000ms (реже, т.к. probe логи каждую секунду)
        if (now - lastLog >= 2000 && now - lastProbe > 100) {
          uint32_t elapsed = now - startTime;
          DEBUG_I("  Polling progress: t=%dms, checks=%lu, probes=%lu, available=%d", (int)elapsed, checks, probeAttempts, Serial1.available());
          lastLog = now;
        }

        // Минимальная задержка 1ms для частой проверки
        delay(1);
      }

      if (uartSerialPtr == nullptr) {
        DEBUG_I("✗ Cradle Port0 timeout (no data, checks=%lu, probes=%lu)", checks, probeAttempts);
      }
    }

    // Гарантируем минимум 3000ms на заставку
    uint32_t welcomeElapsed = millis() - welcomeStartTime;
    if (welcomeElapsed < minWelcomeTime) {
      uint32_t remainingTime = minWelcomeTime - welcomeElapsed;
      DEBUG_I("Welcome screen: holding for %dms more (min 3000ms)", (int)remainingTime);
      delay(remainingTime);
    }
    DEBUG_I("Welcome screen done (shown for %dms)", (int)(millis() - welcomeStartTime));

    ui.jumpTo(MENU_ROOT, /*enterChild=*/false);

    // ----------------- ADCbuttons -----------------
    adc_btns.addButton(20, 1);
    adc_btns.addButton(3100, 2);
    adc_btns.setHoldDuration(1000, 2000);
  } else {
    DEBUG_I("Port3 != SCREEN, display skipped");

    uint32_t initStartTime = millis();
    const uint32_t minInitTime = 3000; // Минимум 3 секунды на инициализацию (как заставка)

    // Без заставки — ждём до 10000ms, отправляя Hello Request каждую секунду
    {
      uint32_t deadline = millis() + 10000;
      uint32_t startTime = millis();
      uint32_t lastLog = startTime;
      uint32_t lastProbe = 0;
      uint32_t probeInterval = 1000; // Отправляем probe каждую секунду
      uint32_t checks = 0;
      uint32_t probeAttempts = 0;

      DEBUG_I("=== Cradle Port0 detection started (timeout=10000ms, no screen, probe interval=1000ms) ===");

      while (millis() < deadline) {
        checks++;
        uint32_t now = millis();

        // Отправляем Hello Request probe каждые probeInterval ms
        if (now - lastProbe >= probeInterval) {
          idryer::UartHelloPayload probe = buildHelloPayload(menu, wtc);
          uartBridge.sendHello(probe, false);
          probeAttempts++;
          lastProbe = now;
          uint32_t elapsed = now - startTime;
          DEBUG_I("  Probe #%lu sent at t=%dms", probeAttempts, (int)elapsed);
        }

        // Проверяем ответ
        if (Serial1.available() > 0) {
          uartSerialPtr = &uartSerial1;
          uint32_t elapsed = millis() - startTime;
          DEBUG_I("✓ Cradle: Link detected on GPIO%d/GPIO%d (Port0, t=%dms, checks=%lu, probes=%lu)", CRADLE_TX_PIN, CRADLE_RX_PIN, (int)elapsed, checks, probeAttempts);
          break;
        }

        // Логируем прогресс каждые 2000ms (реже, т.к. probe логи каждую секунду)
        if (now - lastLog >= 2000 && now - lastProbe > 100) {
          uint32_t elapsed = now - startTime;
          DEBUG_I("  Polling progress: t=%dms, checks=%lu, probes=%lu, available=%d", (int)elapsed, checks, probeAttempts, Serial1.available());
          lastLog = now;
        }

        // Минимальная задержка 1ms для частой проверки
        delay(1);
      }

      if (uartSerialPtr == nullptr) {
        DEBUG_I("✗ Cradle Port0 timeout (no data, checks=%lu, probes=%lu)", checks, probeAttempts);
      }
    }

    // Гарантируем минимум 3000ms на инициализацию (единообразный тайминг)
    uint32_t initElapsed = millis() - initStartTime;
    if (initElapsed < minInitTime) {
      uint32_t remainingTime = minInitTime - initElapsed;
      DEBUG_I("Init delay: holding for %dms more (min 3000ms)", (int)remainingTime);
      delay(remainingTime);
    }
    DEBUG_I("Init done (took %dms)", (int)(millis() - initStartTime));
  }

  DEBUG_I("ADC buttons inited, units_count=%u", menu.units_count);

  // ----------------- UART Bridge ESP32 -----------------
  // КРИТИЧНО: Инициализируем UART ДО SoftPWM чтобы избежать конфликта с ISR
  DEBUG_I("=== Port decision: uartSerialPtr=%s ===", uartSerialPtr != nullptr ? "SET (Port0)" : "NULL (fallback to menu)");

  if (uartSerialPtr != nullptr) {
    // Кроватка (Port0, GPIO16/17) — Link уже обнаружен во время заставки
    setCradleLink(true);
    DEBUG_I("✓ Using cradle port (Port0)");
  } else {
    // Кроватка пуста — берём порт из меню
    Serial1.end(); // освобождаем GPIO16/17
    uint8_t linkPort = getLinkPort();
    DEBUG_I("Cradle empty, switching to menu port: %u", linkPort);
    delay(50);

    if (linkPort == 3) {
      uartSerialPtr = &uartSerial3;
    } else if (linkPort == 2) {
      Serial2.setTX(UART2_TX_PIN);
      Serial2.setRX(UART2_RX_PIN);
      Serial2.setFIFOSize(4096);  // 4КБ: OTA-proxy устойчивость к паузам loop (см. serialPort3)
      uartSerialPtr = &uartSerial2;
    }
  }
  // Для кроватки initUartBridge вызывается повторно — сбрасывает состояние парсера после probe

  initArduinoHal(&Serial);
  if (uartSerialPtr) {
    initUartBridge(uartSerialPtr, UART_BAUD_RATE);
    uartBridge.setRfidDataHandler(handleUartRfidData);
    // DRYER paired OTA — RP receiver: выделяем frag-буфер и монтируем LittleFS.
    ota_rp::begin();
    if (uartSerialPtr == &uartSerial1)
      DEBUG_I("[LINK] Port0 (cradle GPIO%d/GPIO%d) — OK", CRADLE_TX_PIN, CRADLE_RX_PIN);
    else if (uartSerialPtr == &uartSerial2)
      DEBUG_I("[LINK] Port2 (menu, GPIO%d/GPIO%d) — OK", UART2_TX_PIN, UART2_RX_PIN);
    else
      DEBUG_I("[LINK] Port3 (menu, PIO GPIO1/GPIO2) — OK");
  } else {
    DEBUG_I("[LINK] No port assigned — Link not found!");
  }

  // delay(200);
// ----------------- SOFT PWM -----------------
// КРИТИЧНО: запускаем SoftPWM ISR ПОСЛЕ инициализации UART
#ifdef USE_SOFT_PWM
  SoftPwmManager::instance().beginBackend(500); // запуск ISR бэк
  DEBUG_I("SoftPWM inited, units_count=%u", menu.units_count);
#endif
  delay(50);
  // ----------------- WS2812 (NEOPIXEL) -----------------
  ledsBegin();
  DEBUG_I("Neopixel inited, units_count=%u", menu.units_count);
  // delay(200);

  // ----------------- CONTROLLER -----------------
  // HX711Multi создается внутри initHardware() с menu.scales_count
  initHardware();
  DEBUG_I("Hardware inited");
  // delay(200);
  errlog_dump_to_serial();

  /************** PID SETUP ***************/
  uint8_t n = menu.units_count;
  DEBUG_I("PID setup: units_count=%u, NUM_UNITS=%u", n, NUM_UNITS);
  for (uint8_t u = 0; u < n; ++u) {
    DEBUG_I("PID setup for unit %u, controller=%p", u, controllers[u]);
    if (!controllers[u]) {
      DEBUG_E("Controller %u is NULL!", u);
      continue;
    }
    controllers[u]->setHeaterPid(
        /*controller*/ u, PID_ACT_HEATER, menu.pid_kp_heater[u], menu.pid_ki_heater[u], menu.pid_kd_heater[u],
        /*minOut*/ 0.0f,
        /*maxOut*/ 100.0f,
        /*sample*/ PID_UPDATE_INTERVAL_S,
        /*gain_multiplier*/ menu.pid_gain_heater[u]);

    controllers[u]->setAirPid(
        /*controller*/ u, PID_ACT_AIR, menu.pid_kp_chamber[u], menu.pid_ki_chamber[u], menu.pid_kd_chamber[u],
        /*minOut*/ 0.0f,
        /*maxOut*/ menu.heater_max_temp[u], // menu.air_max_temp[u],
        /*sample*/ PID_UPDATE_INTERVAL_S,
        /*gain_multiplier*/ menu.pid_gain_chamber[u]

    );
    DEBUG_I("PID setup done for unit %u", u);
  }

  gscr_active_unit = 0;
  gscr_next_unit_rotate_ms = millis() + UNIT_ROTATE_MS;
  for (uint8_t i = 0; i < n; ++i)
    gscr_units[i].nextSwapMs = millis() + MODE_SWAP_MS;

  for (uint8_t i = 0; i < n; ++i) {
    controllers[i]->applyServoFromMenu(i, true);
  }

  DEBUG_I("Setup done");

  if (hasLink()) {
    sendHello(menu, wtc, false);
  }

  /************ ERROR OVERLAY ************/
  // Если есть ошибки - активируем оверлей, но НЕ блокируем setup()!
  // Экран ошибок будет показан через uiTick() в loop()
  // LED управляется через errorReportTick() в serviceTasks()
  if (errlog_count() > 0) {
    error_ack_begin();
    DEBUG_W("Errors in EEPROM, overlay activated. Count=%u", errlog_count());
  }


  // /***************** RFID ***************/
  // scanI2C(Wire, "Wire");
  // /***************** RFID ***************/

  watchdog_enable(4000, false);

  // DEBUG_I("[SELFTEST] MAIN INFO should print");
  // DEBUG_T("[SELFTEST] MAIN TRACE should print");
  // DEBUG_W("[SELFTEST] MAIN WARNING should print");
  // DEBUG_E("[SELFTEST] MAIN ERROR should print");
  // DEBUG_C("[SELFTEST] MAIN CRITICAL should print");

#if defined(TEST_FAN_ONLY) || defined(TEST_FAN_AND_HEATER)
  pinMode(FAN0, OUTPUT);
  digitalWrite(FAN0, HIGH);
#endif
#if defined(TEST_HEATER_ONLY) || defined(TEST_FAN_AND_HEATER)
  pinMode(HEATER0, OUTPUT);
  digitalWrite(HEATER0, HIGH);
#endif
}

void loop() {

  // uint16_t raw = analogRead(CON);
  // if (raw < 4000) {
  //   DEBUG_I("ADC READ: %u", raw);
  // }

  // uint8_t id = adc_btns.readButton();
  // if (id != 0) {
  //   DEBUG_W("Button: %u", id);
  // }

  watchdog_update();

#if !defined(TEST_FAN_ONLY) && !defined(TEST_HEATER_ONLY) && !defined(TEST_FAN_AND_HEATER)

  serviceSerialDebugCommands();

  if (hasLink()) {
#if defined(ARDUINO_ARCH_RP2040)
    if (getLinkPort() == 2 && Serial2.overflow()) DEBUG_C("[UART] RX BUFFER OVERFLOW!");
    if (getLinkPort() == 3 && serialPort3.overflow()) DEBUG_C("[UART] RX BUFFER OVERFLOW!");
#endif
    uartBridge.loop();
    watchdog_update();
    handlePendingUartCommands();
  }

  safety_supervisor_tick(millis());

  uint32_t now = millis();

  if (hasScreen()) {
    int8_t delta = enc->tick();
    bool back = enc_btn->held();
    bool click = enc_btn->released();
    if (delta || back || click) markUserInput(gscr_active_unit, now);
    ui.handleInput(delta, click, back);
  } else {
    ui.handleInput(0, false, false);
  }

  // Обновить гейт логов: если хотя бы один контроллер не IDLE — verbose off
  bool anyActive = false;
  {
    for (uint8_t i = 0; i < menu.units_count && !anyActive; i++) {
      if (controllers[i] && controllers[i]->mode() != DryerMode::Idle) anyActive = true;
    }
    g_verbose_logs = !anyActive;
    // Синхронизируем уровень HAL-логгера с тем же гейтом
    if (g_hal && g_hal->logger) {
      g_hal->logger->setLevel(idryer::hal::LogLevel::Warning);
    }
  }

  // Paired OTA Этап 5 — commit-gate: проверка готовности (self-healing или
  // paired-flow) + idle → инициация PicoOTA.commit + rp2040.reboot.
  ota_rp::tickCommitGate(millis(), /*allUnitsIdle=*/!anyActive);

  for (uint8_t i = 0; i < menu.units_count; i++) { // для каждого контроллера menu.units_count
    // Датчики читаем всегда, включая режим Error: при обрыве readDryerInputs
    // вернёт NaN → телеметрия отдаст sentinel «нет данных» (разрыв графика на
    // портале), а не замороженное последнее значение. В Error tick() всё равно
    // рано выходит (emergencyStop), inputs_ идёт только в телеметрию.
    DryerInputs in = readDryerInputs(i, now);
    controllers[i]->setInputs(in);
    controllers[i]->tick();
  }

  // ------------------ SERVICE SCREEN'S ----------------------

  watchdog_update();
  uiTick(now, ui);
  ledsTick(now);
  serviceTasks(now); // Обработка сервисных задач

#endif
}


void serviceTasks(uint32_t now) {
  wtc.tick();
  if (enc_btn) enc_btn->tick(); // enc_btn создается только если Port3=SCREEN
  if (hasScreen()) adc_btns.tick(now);
  handleAnalogButtons(now); // Обработка аналоговых кнопок

  errorReportTick(now);

  // LED индикация WiFi/MQTT переходов обрабатывается в handleUartHeartbeat()
  // через отслеживание cloudState переходов

  if (hx711MultiPtr) hx711Multi.readMassMulti(menu.scales_count);

  // RFID-поллинг блокирует main loop и ломает тайминги LED/PID.
  // Опрашиваем ридеры только если все контроллеры в Idle.
  // Активные job'ы (read/write/preview), запущенные ранее, продолжают работать —
  // гардим именно новый детект-поллинг.
  bool allUnitsIdle = true;
  for (uint8_t i = 0; i < menu.units_count; i++) {
    if (controllers[i] && controllers[i]->mode() != DryerMode::Idle) {
      allUnitsIdle = false;
      break;
    }
  }

  // g_g_rfidLastTag / g_g_rfidTagPresent — file-scope (нужны в handlePendingUartCommands)
  for (uint8_t r = 0; r < rfidManager.readerCount(); r++) {
    if (!rfidManager.isAvailable(r)) continue;
    // Снимаем pending если протух по таймауту
    if (g_rfidWritePending[r] && (int32_t)(millis() - g_rfidWritePendingUntilMs[r]) > 0) {
      g_rfidWritePending[r] = false;
    }
    if (g_rfidPreviewJobs[r].active || g_rfidReadJobs[r].active ||
        g_rfidWriteJobs[r].active || g_rfidWriteStaging[r].active ||
        g_rfidWritePending[r]) {
      continue;
    }
    if (g_rfidTagPresent[r] && g_rfidSelfTestArmed[r] && !g_rfidSelfTestDone[r]) {
      continue;
    }
    if (!allUnitsIdle) continue;
    if (now < g_rfidNextPollAtMs[r]) continue;
    g_rfidNextPollAtMs[r] = now + RFID_POLL_INTERVAL_MS;
    uint32_t tagEventStartedAt = millis();
    RfidTag tag;
    RfidStatus pollStatus = rfidManager.poll(r, tag);
    bool found = (pollStatus == RfidStatus::OK);
    bool unchanged = (pollStatus == RfidStatus::UNCHANGED);
    bool noTag = (pollStatus == RfidStatus::NO_TAG);
    if (found || unchanged) {
      g_rfidNoTagStreak[r] = 0;
      g_rfidNoTagSinceMs[r] = 0;
    }
    if (found && !g_rfidTagPresent[r]) {
      g_rfidTagPresent[r] = true;
      g_rfidLastTag[r] = tag;

      // UID
      Serial.printf("[RFID] reader%u unit%u DETECTED uid=", r, rfidManager.unitId(r));
      for (uint8_t b = 0; b < tag.uidLen; b++)
        Serial.printf("%s%02X", b ? ":" : "", tag.uid[b]);

      // Тип
      const char *typeName = "UNKNOWN";
      switch (tag.type) {
      case RfidTagType::TOPAZ_GENERIC:
        typeName = "TOPAZ";
        break;
      case RfidTagType::MIFARE_CLASSIC_1K:
        typeName = "MIFARE_1K";
        break;
      case RfidTagType::ISO14443A_GENERIC:
        typeName = "ISO14443A";
        break;
      case RfidTagType::ISO14443B_GENERIC:
        typeName = "ISO14443B";
        break;
      case RfidTagType::FELICA_GENERIC:
        typeName = "FELICA";
        break;
      case RfidTagType::ISO15693_GENERIC:
        typeName = "ISO15693";
        break;
      case RfidTagType::NTAG213:
        typeName = "NTAG213";
        break;
      case RfidTagType::NTAG215:
        typeName = "NTAG215";
        break;
      case RfidTagType::NTAG216:
        typeName = "NTAG216";
        break;
      case RfidTagType::ICODE_SLIX2:
        typeName = "ICODE_SLIX2";
        break;
      default:
        break;
      }
      Serial.printf(" type=%s blockSize=%u totalBlocks=%u userBlocks=%u startBlock=%u\n",
                    typeName,
                    (unsigned)tag.blockSize,
                    (unsigned)tag.totalBlocks,
                    (unsigned)tag.userBlocks,
                    (unsigned)tag.firstUserBlock);

      // Тяжёлые raw reads не выполняем в пути DETECTED, иначе рвётся основной loop и дыхание LED.
      if (rfidTagReadableBytes(tag) == 0) {
        Serial.println("[RFID] preview skipped: tag detected, raw read is not implemented for this type");
      } else {
        rfidStartPreviewJob(r, tag);
        Serial.println("[RFID] preview scheduled");
      }
      // Отправить TagDetected на ESP32
      {
        uint32_t notifyStartedAt = millis();
        idryer::UartRfidPayload rp{};
        rp.event = static_cast<uint8_t>(idryer::UartRfidEvent::TagDetected);
        rp.readerId = r;
        rp.unitId = rfidManager.unitId(r);
        // UID → HEX-строка "AA:BB:CC:..."
        int off = 0;
        for (uint8_t b = 0; b < tag.uidLen && off < 31; b++) {
          off += snprintf(rp.tag + off, sizeof(rp.tag) - off, b ? ":%02X" : "%02X", tag.uid[b]);
        }
        uartBridge.sendRfid(rp, false);
        ledsShowWebBreath(LedColors::PURPLE, 1000, 3000);
        Serial.printf("[RFID] TagDetected sent tag=%s\n", rp.tag);
        DEBUG_I("[RFID] TagDetected notify reader=%u in %lums totalEvent=%lums",
                r,
                (unsigned long)(millis() - notifyStartedAt),
                (unsigned long)(millis() - tagEventStartedAt));
        // Содержимое метки читается отдельно по команде ReadRfid.
        // На событии TagDetected сообщаем только UID/тип и наличие метки.
        // Auto-self-test отключён на время отладки write-path (перетирает содержимое метки).
        g_rfidSelfTestArmed[r] = false;
        g_rfidSelfTestDone[r] = true;
        g_rfidSelfTestDetectedAtMs[r] = millis();
      }

    } else if (noTag && g_rfidTagPresent[r]) {
      if (g_rfidNoTagStreak[r] == 0) g_rfidNoTagSinceMs[r] = millis();
      if (g_rfidNoTagStreak[r] < 0xFF) g_rfidNoTagStreak[r]++;
      uint32_t noTagMs = millis() - g_rfidNoTagSinceMs[r];
      if (g_rfidNoTagStreak[r] < RFID_TAG_LOST_STREAK || noTagMs < RFID_TAG_LOST_MIN_MS) {
        continue;
      }

      // Сброс состояния — всегда, чтобы новая метка детектировалась корректно.
      g_rfidTagPresent[r] = false;
      g_rfidSelfTestArmed[r] = false;
      g_rfidSelfTestDone[r] = false;
      g_rfidSelfTestLatched[r] = false;
      g_rfidSelfTestLatchedTag[r] = {};
      g_rfidNoTagStreak[r] = 0;
      g_rfidNoTagSinceMs[r] = 0;
      Serial.printf("[RFID] reader%u unit%u TAG REMOVED\n", r, rfidManager.unitId(r));
      idryer::UartRfidPayload rp{};
      rp.event = static_cast<uint8_t>(idryer::UartRfidEvent::TagRemoved);
      rp.readerId = r;
      rp.unitId = rfidManager.unitId(r);
      uartBridge.sendRfid(rp, false);
      ledsShowWebBreath(LedColors::PURPLE, 2000, 1900);
      Serial.printf("[RFID] TagRemoved sent (debounced %u hits, %lums)\n",
                    (unsigned)RFID_TAG_LOST_STREAK,
                    (unsigned long)noTagMs);
    }
  }

  rfidServicePreviewJobs();

  for (uint8_t r = 0; r < RFID_MAX_READERS; r++) {
    if (r >= rfidManager.readerCount()) break;
    if (!g_rfidSelfTestArmed[r] || g_rfidSelfTestDone[r]) continue;
    if (!g_rfidTagPresent[r]) continue;
    if (g_rfidPreviewJobs[r].active || g_rfidReadJobs[r].active || g_rfidWriteJobs[r].active) continue;
    if (millis() - g_rfidSelfTestDetectedAtMs[r] < RFID_SELF_TEST_SETTLE_MS) continue;

    Serial.printf("[RFIDTEST] auto-start reader=%u after %lums stable tag\n",
                  r,
                  (unsigned long)(millis() - g_rfidSelfTestDetectedAtMs[r]));
    bool ok = rfidRunWriteSelfTest(r);
    g_rfidSelfTestDone[r] = true;
    g_rfidSelfTestArmed[r] = false;
    g_rfidSelfTestLatched[r] = true;
    g_rfidSelfTestLatchedTag[r] = g_rfidLastTag[r];
    Serial.printf("[RFIDTEST] auto-finish reader=%u result=%s\n", r, ok ? "OK" : "FAIL");
  }

  rfidServiceReadJobs();
  rfidServiceWriteJobs();

  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 60000) {
    lastPrint = millis();
    DEBUG_W("Hours: %lu, Minutes: %u", wtc.getHours(), wtc.getMinutes());
  }

  if (hasLink()) {
    static uint32_t lastTelemetrySent = 0;
    static uint32_t lastStatusSent = 0;
    static uint32_t lastWeightsSent = 0;
    static uint32_t lastHeartbeatSent = 0;

    if (now - lastTelemetrySent >= UART_TELEMETRY_INTERVAL_MS) {
      sendUartTelemetry();
      lastTelemetrySent = now;
    }
    if (now - lastStatusSent >= UART_STATUS_INTERVAL_MS) {
      sendUartStatus();
      lastStatusSent = now;
    }
    if (now - lastWeightsSent >= UART_WEIGHTS_INTERVAL_MS) {
      sendUartWeights();
      lastWeightsSent = now;
    }
    if (now - lastHeartbeatSent >= UART_HEARTBEAT_INTERVAL_MS) {
      sendUartHeartbeat();
      lastHeartbeatSent = now;
    }
  }
}


void handlePendingUartCommands() {
  if (!hasLink()) return;
  PendingUartCommand cmd;

  // Обрабатываем все команды из очереди
  while (dequeueUartCommand(&cmd)) {
    switch (cmd.code) {
    case idryer::UartCmdCode::Find: {
      // Мигание LED или экрана для идентификации устройства
      uint8_t unit = cmd.unitId;
      DEBUG_W("[MAIN] FIND command for unit %d - blinking", unit);
      // ledsShowAlert(LedColors::GREEN, 2000);
      ledsShowAlertBreath(LedColors::CYAN, 2000, 10000);
      break;
    }

    case idryer::UartCmdCode::ReadRfid: {
      uint32_t cmdStartedAt = millis();
      uint8_t unit = cmd.unitId;
      DEBUG_W("[MAIN] READ_RFID unit=%u", unit);

      // Найти ридер для этого unit
      uint8_t readerId = 0xFF;
      for (uint8_t r = 0; r < rfidManager.readerCount(); r++) {
        if (rfidManager.isAvailable(r) && rfidManager.unitId(r) == unit) {
          readerId = r;
          break;
        }
      }
      if (readerId == 0xFF) {
        DEBUG_W("[MAIN] READ_RFID: no reader for unit=%u", unit);
        break;
      }
      if (!g_rfidTagPresent[readerId]) {
        DEBUG_W("[MAIN] READ_RFID: no tag on reader=%u", readerId);
        break;
      }
      rfidRequestReadTagData(readerId, RfidTransferMode::Async);
      DEBUG_I("[MAIN] READ_RFID scheduled reader=%u in %lums",
              readerId, (unsigned long)(millis() - cmdStartedAt));
      break;
    }

    case idryer::UartCmdCode::WriteRfid: {
      uint8_t unit = cmd.unitId;
      uint32_t expectedSize = cmd.arg0;   // размер данных в байтах
      uint32_t verifyCode = cmd.arg1;     // 0=None, 1=Header32, 2=Full (default: Header32)
      DEBUG_W("[MAIN] WRITE_RFID unit=%u size=%u verify=%u", unit,
              (unsigned)expectedSize, (unsigned)verifyCode);

      if (expectedSize == 0 || expectedSize > RFID_DATA_SIZE) {
        DEBUG_W("[MAIN] WRITE_RFID: invalid size=%u", (unsigned)expectedSize);
        break;
      }

      // Найти ридер для этого unit.
      uint8_t readerId = 0xFF;
      for (uint8_t r = 0; r < rfidManager.readerCount(); r++) {
        if (rfidManager.isAvailable(r) && rfidManager.unitId(r) == unit) {
          readerId = r;
          break;
        }
      }
      if (readerId == 0xFF) {
        DEBUG_W("[MAIN] WRITE_RFID: no reader for unit=%u", unit);
        break;
      }
      if (!g_rfidTagPresent[readerId]) {
        DEBUG_W("[MAIN] WRITE_RFID: no tag on reader=%u", readerId);
        break;
      }

      RfidVerifyMode verify = (verifyCode == 0) ? RfidVerifyMode::None
                              : (verifyCode == 2) ? RfidVerifyMode::Full
                                                  : RfidVerifyMode::Header32;

      bool armed = rfidArmWriteStaging(readerId, expectedSize, verify);
      DEBUG_I("[MAIN] WRITE_RFID staging armed=%s reader=%u size=%u",
              armed ? "OK" : "FAIL", readerId, (unsigned)expectedSize);
      break;
    }

    case idryer::UartCmdCode::ResetFault: {
      uint8_t unit = cmd.unitId;
      DEBUG_W("[MAIN] RESET_FAULT command for unit %d", unit);

      // Сбрасываем ошибку контроллера
      if (unit < NUM_UNITS && controllers[unit]) {
        // TODO: добавить метод resetFault() в DryerController
        // controllers[unit]->resetFault();
        DEBUG_I("[MAIN] Fault reset for unit %d", unit);
      }
      break;
    }

    case idryer::UartCmdCode::ClearErrors: {
      DEBUG_W("[MAIN] CLEAR_ERRORS command");

      errlog_clear();    // очистка EEPROM лога
      error_ack_begin(); // сброс состояния оверлея (сразу выйдем из ожидания)

      // Разблокируем все контроллеры из режима Error
      for (uint8_t i = 0; i < menu.units_count; i++) {
        if (controllers[i] && controllers[i]->mode() == DryerMode::Error) {
          controllers[i]->stop(); // Переводим в Idle и безопасно выключаем
          DEBUG_I("[MAIN] Unit %d reset from Error to Idle", i);
        }
      }

      ledsShowAlertBreath(LedColors::GREEN, 1200, 3000);
      DEBUG_I("[MAIN] Error log cleared via UART");
      break;
    }

    default:
      DEBUG_E("[MAIN] Unknown pending command: %d", (int)cmd.code);
      break;
    }
  }
}


// Троттлинг отправки повторяющихся ошибок по UART: одинаковое (src,code,ctrl_id)
// уходит не чаще раза в ERR_SEND_COOLDOWN_MS, первое событие — сразу. Гасит спам
// при обрыве/замыкании датчика (SENSOR_INVALID постится каждую итерацию loop).
// INFO (например STATE_CHANGE) не троттлится — это единичные легитимные события.
// errlog_append_from_event и ACK обрабатываются всегда (дедуп count живёт в errlog).
static bool sendUartErrorEventThrottled(const ErrorEvent *ev, uint32_t now) {
  if (ev->severity == ERRSEV_INFO) return sendUartErrorEvent(ev);

  static const uint32_t ERR_SEND_COOLDOWN_MS = 5000;
  static const uint8_t SLOTS = 8;
  static struct { uint16_t code; uint8_t src; uint8_t ctrl; uint32_t last; bool used; } recent[SLOTS] = {};

  int slot = -1, freeSlot = -1;
  for (uint8_t i = 0; i < SLOTS; ++i) {
    if (recent[i].used && recent[i].code == (uint16_t)ev->code &&
        recent[i].src == (uint8_t)ev->source && recent[i].ctrl == ev->ctrl_id) { slot = (int)i; break; }
    if (!recent[i].used && freeSlot < 0) freeSlot = (int)i;
  }

  if (slot >= 0) {
    if (now - recent[slot].last < ERR_SEND_COOLDOWN_MS) return false; // повтор — подавляем
    recent[slot].last = now;
  } else {
    uint8_t i = (freeSlot >= 0) ? (uint8_t)freeSlot : (uint8_t)(now % SLOTS); // нет места — вытесняем
    recent[i] = { (uint16_t)ev->code, (uint8_t)ev->source, ev->ctrl_id, now, true };
  }
  return sendUartErrorEvent(ev);
}

void safety_supervisor_tick(uint32_t now) {
  static uint32_t last_check = 0;
  if (now - last_check < 100) return;
  last_check = now;

  ErrorEvent ev;
  char line[160];
  while (errorbus_poll(&ev)) {
    watchdog_update();
    error_format_line(&ev, line, sizeof(line));
    Serial.printf("[ERRBUS] live %s\n", line);
    // DEBUG_E("[SAFETY] %s", line);
    switch (ev.severity) {
    case ERRSEV_CRITICAL: {
      DEBUG_C("%s", line);
      const uint8_t n = menu.units_count;
      for (uint8_t u = 0; u < n; ++u) {
        auto *ctrl = controllers[u];
        ctrl->emergencyStop();
        ctrl->setMode(DryerMode::Error);
      }

      sendUartErrorEventThrottled(&ev, now);
      ledsShowAlertBreath(LedColors::RED, 500, 1000 * 60 * 60 * 24);
      DEBUG_C("All units stopped due to critical error");
      break;
    }
    case ERRSEV_ERROR: {
      DEBUG_E("%s", line);
      auto *ctrl = controllers[ev.ctrl_id];
      ctrl->emergencyStop();
      ctrl->setMode(DryerMode::Idle);
      sendUartErrorEventThrottled(&ev, now);
      ledsShowAlertBreath(LedColors::ORANGE, 3000, 9000);
      DEBUG_E("Stopping unit %u due to error", ev.ctrl_id);
      break;
    }
    case ERRSEV_WARNING:
      DEBUG_W("%s", line);
      sendUartErrorEventThrottled(&ev, now);
      ledsShowAlertBreath(LedColors::YELLOW, 3000, 3000);
      break;
    case ERRSEV_INFO:
      sendUartErrorEventThrottled(&ev, now);
      DEBUG_I("%s", line);
      break;
    default:
      break;
    }
    // 4) Запись в EEPROM по серьезности ошибки(фильтр внутри)
    errlog_append_from_event(&ev);
    // 5) ACK ВСЕГДА, для любого severity
    error_ack_begin();
  }
}
