#include "hardware.h"
#include "../RFID/GpioCs.h"
#include "../RFID/drivers/Pn5180Driver.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_HADWARE
#define LOG_TAG "HADWARE"
#include "debug_log.h"

extern MenuState menu;

// ----------------- RFID -----------------
static GpioCs      rfidCs0(RFID_CS0);
static Pn5180Driver pn5180_0(SPI1, rfidCs0, RFID_BUSY0, RFID_RST, RFID_MISO, RFID_MOSI, RFID_SCK);

// ----------------- SCALES -----------------
// Объект создаётся в initHardware() по getScalesPort()
HX711Multi *hx711MultiPtr = nullptr;

// ----------------- CONTROLLERS -----------------
DryerController iDryer0(HEATER0, FAN0, SERVO0, 256, 5, 0);
DryerController iDryer1(HEATER1, FAN1, SERVO1, 256, 5, 1);
DryerController iDryer2(HEATER2, FAN2, SERVO2, 256, 5, 2);
DryerController *controllers[3] = {&iDryer0, &iDryer1, &iDryer2};

// ----------------- HEATER SENSORS -----------------
auto makeHeaterSensor0() { return ThermistorSensor::byType(THERMISTOR0, HEATER_UNIT0_THERMISTOR_MODEL, PULLUP_UNIT0, INLINE_RESISTOR_UNIT0, ALPHA); }
auto makeHeaterSensor1() { return ThermistorSensor::byType(THERMISTOR1, HEATER_UNIT1_THERMISTOR_MODEL, PULLUP_UNIT1, INLINE_RESISTOR_UNIT1, ALPHA); }
ThermistorSensor heaterSensor0 = makeHeaterSensor0();
ThermistorSensor heaterSensor1 = makeHeaterSensor1();

auto makeHeaterSensor2() { return ThermistorSensor::byType(THERMISTOR2, HEATER_UNIT2_THERMISTOR_MODEL, PULLUP_UNIT2, INLINE_RESISTOR_UNIT2, ALPHA); }
ThermistorSensor heaterSensor2 = makeHeaterSensor2();
ThermistorSensor *heaterSensors[3] = {&heaterSensor0, &heaterSensor1, &heaterSensor2};

uint8_t scanI2C(TwoWire &wire, const char *name) {
  Serial.printf("\nScanning I2C bus %s...\n", name);
  uint8_t count = 0;

  // безопасный диапазон 7-битных адресов (0x00..0x07 и 0x78..0x7F —
  // зарезервированы)
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    wire.beginTransmission(addr);
    uint8_t err = wire.endTransmission();
    if (err == 0) {
      Serial.printf("I2C device at 7-bit 0x%02X (write 0x%02X, read 0x%02X)\n", addr, (uint8_t)(addr << 1), (uint8_t)((addr << 1) | 1));
      count++;
    } else if (err == 4) {
      Serial.printf("Unknown error at 0x%02X\n", addr);
    }
  }
  if (!count) Serial.printf("No I2C devices found on bus %s\n", name);
  return count;
}

Sht31Sensor airSensor0(SHT31_ADDR_UNIT0, &Wire);
Sht31Sensor airSensor1(SHT31_ADDR_UNIT1, &Wire);
Sht31Sensor airSensor2(SHT31_ADDR_UNIT2, &Wire1);
Sht31Sensor *airSensors[3] = {&airSensor0, &airSensor1, &airSensor2};

static void initShtWithLog(uint8_t idx, Sht31Sensor &s, const char *name, float rate_hz) {
  Serial.print("[INIT] ");
  Serial.print(name);
  Serial.print(": ");
  if (!s.begin()) {
    auto r = s.get();
    Serial.print("FAILED, err=");
    Serial.print(r.err);
    Serial.print(" (");
    Serial.print(shtErrToStr(r.err));
    Serial.println(")");
  } else {
    s.setRateHz(rate_hz);
    delay(50); // время на инициализацию
    Serial.println("OK");
  }
}

// ----------------- INIT -----------------
void initHardware() {
  extern MenuState menu;
  DEBUG_I("[initHardware] START: units_count=%u", menu.units_count);

  const uint8_t N = active_units();
  DEBUG_I("[initHardware] N=%u (active_units)", N);

  if (N >= 1) {
    heaterSensor0.begin();
    heaterSensor0.setRateHz(SAMPLE_RATE_HZ);
  }
  if (N >= 2) {
    heaterSensor1.begin();
    heaterSensor1.setRateHz(SAMPLE_RATE_HZ);
  }
  if (N >= 3) {
    heaterSensor2.begin();
    heaterSensor2.setRateHz(SAMPLE_RATE_HZ);
  }
  // Первичная проверка статуса датчиков нагревателя только активные
  uint32_t nowMs = millis();

  // get() SensorReading last_ SensorReading;
  if (N >= 1) {
    auto rd0 = heaterSensor0.get();
    report_sensor_error(0, ERRSRC_THERM, rd0, nowMs);
  }
  if (N >= 2) {
    auto rd1 = heaterSensor1.get();
    report_sensor_error(1, ERRSRC_THERM, rd1, nowMs);
  }
  if (N >= 3) {
    auto rd2 = heaterSensor2.get();
    report_sensor_error(2, ERRSRC_THERM, rd2, nowMs);
  }
  // AIR SENSORS
  Serial.println("[INIT] SHT31 I2C addresses:");
  Serial.printf("  UNIT0: 0x%02x, UNIT1: 0x%02x, UNIT2: 0x%02x\n", SHT31_ADDR_UNIT0, SHT31_ADDR_UNIT1, SHT31_ADDR_UNIT2);

  // Используем sprintf для форматирования адресов в runtime
  char sht0_name[32], sht1_name[32], sht2_name[32];
  snprintf(sht0_name, sizeof(sht0_name), "SHT0(Wire,0x%02x)", SHT31_ADDR_UNIT0);
  snprintf(sht1_name, sizeof(sht1_name), "SHT1(Wire1,0x%02x)", SHT31_ADDR_UNIT1);
  snprintf(sht2_name, sizeof(sht2_name), "SHT2(Wire1,0x%02x)", SHT31_ADDR_UNIT2);

  if (N >= 1) initShtWithLog(0, airSensor0, sht0_name, 10.0f);
  if (N >= 2) initShtWithLog(1, airSensor1, sht1_name, 10.0f);
  if (N >= 3) initShtWithLog(2, airSensor2, sht2_name, 10.0f);

  // get() SensorReading last_ SensorReading;
  if (N >= 1) {
    auto rd0 = airSensor0.get();
    report_sensor_error(0, ERRSRC_SHT, rd0, nowMs);
  }
  if (N >= 2) {
    auto rd1 = airSensor1.get();
    report_sensor_error(1, ERRSRC_SHT, rd1, nowMs);
  }
  if (N >= 3) {
    auto rd2 = airSensor2.get();
    report_sensor_error(2, ERRSRC_SHT, rd2, nowMs);
  }
  // Контроллеры
  if (N >= 1) iDryer0.begin();
  if (N >= 2) iDryer1.begin();
  if (N >= 3) iDryer2.begin();

  // HX711 init по рантайм-конфигурации портов
  uint8_t scalesPort = getScalesPort();
  if (scalesPort == 1) {
    hx711MultiPtr = new HX711Multi(menu.scales_count, HEATER1, FAN1, SERVO1, THERMISTOR1);
  } else if (scalesPort == 2) {
    hx711MultiPtr = new HX711Multi(menu.scales_count, HEATER2, FAN2, SERVO2, THERMISTOR2);
  }
  if (hx711MultiPtr) {
    hx711MultiPtr->begin(128);
    DEBUG_I("HX711 inited on Port%u", scalesPort);
  } else {
    DEBUG_I("HX711 not configured (no SCL port)");
  }

  // RFID: single-reader configuration, reader0 -> unit0
  rfidManager.addReader(&pn5180_0, 0);
  rfidManager.begin();
}

// ----------------- READ SENSORS -----------------
DryerInputs readDryerInputs(uint8_t unit, uint32_t now) {
  DryerInputs in{};
  const uint8_t N = active_units();
  if (unit >= N) return in; // неактивный блок - пусто

  airSensors[unit]->tick(now);
  heaterSensors[unit]->tick(now);

  auto a = airSensors[unit]->get();
  auto h = heaterSensors[unit]->get();

  report_sensor_error(unit, ERRSRC_AIR, a, millis());
  report_sensor_error(unit, ERRSRC_HEATER, h, millis());

  // [TEMP-DEBUG] раз в 2 секунды печатаем то, что реально вернул SHT31 —
  // чтобы сравнить с тем, что улетело в MQTT telemetry. Удалить после
  // отладки нулей в telemetry.
  static uint32_t s_lastDbg = 0;
  uint32_t nowMs = millis();
  if (nowMs - s_lastDbg > 2000) {
    s_lastDbg = nowMs;
    // Serial.printf("[SHT] u=%u menu.uc=%u air ok=%d t=%.2f h=%.2f | heater ok=%d t=%.2f\n",
    //               unit, menu.units_count, a.ok, a.temperature, a.humidity, h.ok, h.temperature);
  }

  in.airTempC = a.ok ? a.temperature : NAN;
  in.airHumRH = a.ok ? a.humidity : NAN;
  in.heaterTempC = h.ok ? h.temperature : NAN;
  return in;
}

/**** RETURN THE NUMBER OF CONTROLLERS FROM THE MENU  ****/
static inline uint8_t active_units() {
  uint8_t n = menu.units_count; // <-- имя bind'а из меню
  if (n < 1) n = 1;
  if (n > NUM_UNITS) n = NUM_UNITS;

  return n;
}

// ----------------- PORT CONFIG -----------------
const char *portModeToString(PortMode mode) {
  switch (mode) {
  case PORT_EXT:
    return "EXT";
  case PORT_SCREEN:
    return "SCR";
  case PORT_SCALES:
    return "SCL";
  case PORT_LINK:
    return "LNK";
  default:
    return "???";
  }
}

bool isPortModeValid(uint8_t port, PortMode mode) {
  // Допустимые режимы по портам:
  //   Port1: EXT, SCL
  //   Port2: EXT, SCL, LNK
  //   Port3: SCR, LNK
  switch (port) {
  case 1:
    if (mode != PORT_EXT && mode != PORT_SCALES) return false;
    break;
  case 2:
    if (mode != PORT_EXT && mode != PORT_SCALES && mode != PORT_LINK) return false;
    break;
  case 3:
    if (mode != PORT_SCREEN && mode != PORT_LINK) return false;
    break;
  default:
    return false;
  }

  // Singleton: SCR, SCL, LNK — только один экземпляр во всей системе
  PortMode others[2];
  if (port == 1) {
    others[0] = (PortMode)menu.port2_mode;
    others[1] = (PortMode)menu.port3_mode;
  } else if (port == 2) {
    others[0] = (PortMode)menu.port1_mode;
    others[1] = (PortMode)menu.port3_mode;
  } else {
    others[0] = (PortMode)menu.port1_mode;
    others[1] = (PortMode)menu.port2_mode;
  }

  if (mode == PORT_SCREEN || mode == PORT_SCALES || mode == PORT_LINK) {
    if (others[0] == mode || others[1] == mode) return false;
  }

  // EXT подряд от Port1: Port2=EXT требует Port1=EXT
  if (port == 2 && mode == PORT_EXT && menu.port1_mode != PORT_EXT) return false;

  return true;
}

// Снапшот конфигурации портов (фиксируется при старте, не меняется до ребута)
static uint8_t g_hw_port1 = PORT_EXT;
static uint8_t g_hw_port2 = PORT_EXT;
static uint8_t g_hw_port3 = PORT_SCREEN;
static bool g_hw_cradle_link = false;

void snapshotPortConfig() {
  g_hw_port1 = menu.port1_mode;
  g_hw_port2 = menu.port2_mode;
  g_hw_port3 = menu.port3_mode;
}

void setCradleLink(bool active) { g_hw_cradle_link = active; }

// Runtime queries — все по снапшоту, не по menu.*
bool hasScreen()        { return g_hw_port3 == PORT_SCREEN; }
bool hasLink()          { return g_hw_cradle_link || g_hw_port2 == PORT_LINK || g_hw_port3 == PORT_LINK; }
uint8_t getLinkPort()   { return (g_hw_port2 == PORT_LINK) ? 2 : (g_hw_port3 == PORT_LINK) ? 3 : 0; }
uint8_t getScalesPort() { return (g_hw_port1 == PORT_SCALES) ? 1 : (g_hw_port2 == PORT_SCALES) ? 2 : 0; }

bool isPortExt(uint8_t port) {
  if (port == 0) return true; // Port0 всегда MAIN
  if (port == 1) return g_hw_port1 == PORT_EXT;
  if (port == 2) return g_hw_port2 == PORT_EXT;
  if (port == 3) return g_hw_port3 == PORT_EXT;
  return false;
}

uint8_t calcMaxUnits() {
  uint8_t max = 1; // Port0 всегда MAIN
  if (g_hw_port1 == PORT_EXT) {
    max = 2;
    if (g_hw_port2 == PORT_EXT) max = 3;
  }
  return max;
}

void report_sensor_error(uint8_t idx, ErrSource src, const SensorReading &rd, uint32_t now_ms) {
  if (rd.ok) return;

  if (src == ERRSRC_THERM || src == ERRSRC_HEATER) {
    report_therm_error(idx, src, static_cast<ThermErr>(rd.err), rd, now_ms);
  } else if (src == ERRSRC_SHT) {
    report_sht_error(idx, src, static_cast<ShtErr>(rd.err), rd, now_ms);
  } else {
    ErrCode ec = sensor_err_code_for(src);
    POST_ERROR(ERRSEV_ERROR, idx, src, ec, "Sensor error", (int32_t)rd.err);
  }
}

// Thermistor error reporting
static inline void report_therm_error(uint8_t idx, ErrSource src, ThermErr err, const SensorReading &rd, uint32_t now_ms) {
  ErrCode ec = sensor_err_code_for(src);
  switch (err) {
  case ThermErr::ShortCircuit:
    POST_ERROR(ERRSEV_CRITICAL, idx, src, ERRC_SENSOR_SHORT, "Thermistor short circuit", (int32_t)rd.err);
    break;
  case ThermErr::OpenCircuit:
    DEBUG_E("Thermistor open circuit");
    POST_ERROR(ERRSEV_CRITICAL, idx, src, ERRC_SENSOR_OPEN, "Thermistor open circuit", (int32_t)rd.err);
    break;
  case ThermErr::OutOfRange: {
    DEBUG_E("Thermistor out of range: tC=%.1f", (double)rd.temperature);
    int32_t t = isfinite(rd.temperature) ? (int32_t)lroundf(rd.temperature) : 0;
    POST_ERROR(ERRSEV_ERROR, idx, src, ERRC_OUT_OF_RANGE, "Thermistor out of range", (int32_t)rd.err);
  } break;
  case ThermErr::NoTable:
    DEBUG_E("Thermistor model not set");
    POST_ERROR(ERRSEV_ERROR, idx, src, ec, "Thermistor model not set", (int32_t)rd.err);
    break;
  case ThermErr::OK:
    break;
  }
}

// SHT31 error reporting
static inline void report_sht_error(uint8_t idx, ErrSource src, ShtErr err, const SensorReading &rd, uint32_t now_ms) {
  ErrCode ec = sensor_err_code_for(src);
  switch (err) {
  case ShtErr::NotFound:
    POST_ERROR(ERRSEV_CRITICAL, idx, src, ec, "SHT not found", 0);
    break;
  case ShtErr::NoData:
    POST_ERROR(ERRSEV_ERROR, idx, src, ec, "SHT no data", 0);
    break;
  case ShtErr::ReadFail:

    POST_ERROR(ERRSEV_ERROR, idx, src, ERRC_SENSOR_INVALID, "SHT read failed", 0);
    break;
  case ShtErr::OutOfRange: {
    int32_t t10 = isfinite(rd.temperature) ? (int32_t)lroundf(rd.temperature) : 0;
    POST_ERROR(ERRSEV_CRITICAL, idx, src, ERRC_OUT_OF_RANGE, "SHT out of range", t10);
  } break;
  case ShtErr::OK:
    break;
  }
}
