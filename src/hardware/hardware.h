#pragma once
#include "configuration.h"
#include "controller/control.h"
#include "sensor/Sht31Sensor.h"
#include "sensor/ThermistorSensor.h"
#include <Arduino.h>
#include <math.h>
#include <stdint.h>

#include "HX711/HX711.h"
#include "configuration.h"
#include "error/error_defs.h"
#include "error/error_post.h"
#include "hardware/port_config.h"
#include "menu/menu_state.h"

// SHT31_ADDRESS задается через platformio.ini для каждого окружения
#ifndef SHT31_ADDRESS
#define SHT31_ADDRESS 0x44 // По умолчанию 0x44
#endif

#define SHT31_ADDR_UNIT0 SHT31_ADDRESS                         // I2C12 - конфигурируемый адрес
#define SHT31_ADDR_UNIT1 (SHT31_ADDRESS == 0x44 ? 0x45 : 0x44) // I2C12 - противоположный от UNIT0
#define SHT31_ADDR_UNIT2 0x44                                  // I2C13 - всегда 0x44

// ----------------- HEATERS -----------------
#define HEATER0 9 // GPIO9  H0
#define HEATER1 6 // GPIO6  H2
#define HEATER2 3 // GPIO3  H3
#define HEATER3 0 // GPIO0  H4

// ----------------- FANS -----------------
#define FAN0 10 // GPIO10 FAN0
#define FAN1 7  // GPIO7  FAN1
#define FAN2 4  // GPIO4  FAN2
#define FAN3 1  // GPIO1  FAN3

// ----------------- SERVOS -----------------
#define SERVO0 11 // GPIO11 SRV0
#define SERVO1 8  // GPIO8  SRV1
#define SERVO2 5  // GPIO5  SRV2
#define SERVO3 2  // GPIO2  SRV3

// ----------------- I²C -----------------
#define SDA12 20 // GPIO20
#define SCL12 21 // GPIO21
#define SDA13 22 // GPIO22
#define SCL13 23 // GPIO23

// ----------------- NEOPIXEL -----------------
#define NEOPIXEL 25 // GPIO25

// ----------------- THERMISTORS -----------------
#define THERMISTOR0 26 // GPIO26 ADC0 (T3)
#define THERMISTOR1 27 // GPIO27 ADC1 (T2)
#define THERMISTOR2 28 // GPIO28 ADC2 (T1)
#define THERMISTOR3 29 // GPIO29 ADC3 (T0)

//  ----------------- PORT 3 -----------------
#define CON THERMISTOR3  // CONFIRM BTN
#define BACK THERMISTOR3 // BACK BTN
#define SCREEN_SDA SDA13 // I2C SDA
#define SCREEN_SCL SCL13 // I2C SCL
#define PSH SERVO3       // PUSH BTN
#define TRA FAN3         // ENCODER A
#define TRB HEATER3      // ENCODER B

// UART пины - ДЕФОЛТНЫЕ для Serial1/Serial2
// Serial1 (UART0): TX=GPIO0, RX=GPIO1 (Port3)
// Serial2 (UART1): TX=GPIO4, RX=GPIO5 (Port2)
#define UART1_TX_PIN FAN3
#define UART1_RX_PIN SERVO3
#define UART2_TX_PIN FAN2
#define UART2_RX_PIN SERVO2

// ----------------- КРОВАТКА ESP32 (ревизия 2+) -----------------
// Выделенный UART порт для iDryer Link на плате (TXD0/RXD0)
// GPIO16/17 не пересекаются ни с одним другим портом/функцией
#define CRADLE_TX_PIN 16 // GPIO16 TXD0
#define CRADLE_RX_PIN 17 // GPIO17 RXD0


// -----------------  SPI (ревизия 2+) -----------------
#define MISO 12 // GPIO12 MISO (SPI0) - может использоваться для чтения данных с внешних SPI-устройств, если это потребуется в будущем. GPIO12 также может использоваться как вход для других целей, если SPI не используется.
#define SCK 14  // SCK (SPI0) - может использоваться для синхронизации с внешними SPI-устройствами, если это потребуется в будущем. GPIO14 также может использоваться как выход для других целей, если SPI не используется.
#define MOSI 15 // MOSI (SPI0) - может использоваться для передачи данных на внешние SPI-устройства, если это потребуется в будущем. GPIO15 также может использоваться как выход для других целей, если SPI не используется.
#define BUSY 13 // PN5180 BUSY или PN532 IRQ в single-reader конфигурации
#define RST 18  // многофункциональный пин, который может использоваться для сброса внешних устройств или для других целей, в зависимости от конфигурации. GPIO18 также может использоваться как выход для других целей, если не используется для сброса.
#define NSS 19  // многофункциональный пин, который может использоваться как CS для SPI-устройств или для других целей, в зависимости от конфигурации. GPIO19 также может использоваться как выход для других целей, если не используется как CS.

// Алиасы для RFID (SPI1, rev2+)
// PN532: CS=NSS(19), IRQ=BUSY(13), RST(18)
// PN5180: NSS=19, BUSY=13, RST=18
#define RFID_CS0  NSS   // GPIO19
#define RFID_BUSY0 BUSY // GPIO13
#define RFID_IRQ0  BUSY // GPIO13
#define RFID_RST  RST   // GPIO18
#define RFID_MISO MISO  // GPIO12
#define RFID_MOSI MOSI  // GPIO15
#define RFID_SCK  SCK   // GPIO14

// ----------------- RFID -----------------
#include "../RFID/RfidManager.h"

// ----------------- CONTROLLERS -----------------
extern DryerController *controllers[NUM_UNITS];
inline DryerController *get_controller(uint8_t i) { return (i < NUM_UNITS) ? controllers[i] : nullptr; }

// ----------------- HEATER SENSORS -----------------
extern ThermistorSensor *heaterSensors[NUM_UNITS];
inline ThermistorSensor *get_heater_sensor(uint8_t i) { return (i < NUM_UNITS) ? heaterSensors[i] : nullptr; }

// ----------------- AIR (SHT31) SENSORS -----------------
extern Sht31Sensor *airSensors[NUM_UNITS];
inline Sht31Sensor *get_air_sensor(uint8_t i) { return (i < NUM_UNITS) ? airSensors[i] : nullptr; }

extern HX711Multi *hx711MultiPtr;
#define hx711Multi (*hx711MultiPtr)

extern DryerInputs g_inputs;

DryerInputs readDryerInputs(uint8_t unit, uint32_t now);
void initHardware();

static inline ErrCode sensor_err_code_for(ErrSource src) { return (src == ERRSRC_HEATER) ? ERRC_SENSOR_INVALID : ERRC_SENSOR_INVALID; }

void report_sensor_error(uint8_t idx,
                         ErrSource src,           // ERRSRC_HEATER или ERRSRC_AIR
                         const SensorReading &rd, // что вернул сенсор
                         uint32_t now_ms);

static inline uint8_t active_units();
void report_sensor_error(uint8_t idx, ErrSource src, const SensorReading &rd, uint32_t now_ms);

static inline void report_therm_error(uint8_t idx, ErrSource src, ThermErr err, const SensorReading &rd, uint32_t now_ms);

static inline void report_sht_error(uint8_t idx, ErrSource src, ShtErr err, const SensorReading &rd, uint32_t now_ms);

// uint8_t scanI2C(TwoWire *wire, const char *name);
uint8_t scanI2C(TwoWire &wire, const char *name);
