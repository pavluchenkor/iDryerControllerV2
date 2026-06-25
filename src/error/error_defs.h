#pragma once

// Severities: (enum, short name)
#define ERRSEV_LIST(X)                                                                                                                                                                                 \
  X(ERRSEV_INFO, "INFO")                                                                                                                                                                               \
  X(ERRSEV_WARNING, "WARN")                                                                                                                                                                            \
  X(ERRSEV_ERROR, "ERROR")                                                                                                                                                                             \
  X(ERRSEV_CRITICAL, "CRIT")

// Sources: (enum, short name)
#define ERRSRC_LIST(X)                                                                                                                                                                                 \
  X(ERRSRC_CORE, "CORE")                                                                                                                                                                               \
  X(ERRSRC_HEATER, "HEATER")                                                                                                                                                                           \
  X(ERRSRC_AIR, "AIR")                                                                                                                                                                                 \
  X(ERRSRC_THERM, "THERMISTOR")                                                                                                                                                                        \
  X(ERRSRC_SHT, "SHT")                                                                                                                                                                                 \
  X(ERRSRC_SERVO, "SERVO")                                                                                                                                                                             \
  X(ERRSRC_MODE, "MODE")                                                                                                                                                                               \
  X(ERRSRC_STORAGE, "STORAGE_MODE")                                                                                                                                                                    \
  X(ERRSRC_DRYING, "DRYING_MODE")                                                                                                                                                                      \
  X(ERRSRC_PID, "PID_AUTOTUNE")                                                                                                                                                                        \
  X(ERRSRC_UI, "UI")                                                                                                                                                                                   \
  X(ERRSRC_LINK, "LINK")

// Codes: (enum, machine-name, human-text)
#define ERRCODE_LIST(X)                                                                                                                                                                                \
  X(ERRC_OK, "OK", "OK")                                                                                                                                                                               \
  X(ERRC_SENSOR_INVALID, "SENSOR_INVALID", "Sensor reading invalid")                                                                                                                                   \
  X(ERRC_OUT_OF_RANGE, "OUT_OF_RANGE", "Out of range")                                                                                                                                                 \
  X(ERRC_SENSOR_SHORT, "SENSOR_SHORT", "Short circuit")                                                                                                                                                \
  X(ERRC_SENSOR_OPEN, "SENSOR_OPEN", "Open circuit")                                                                                                                                                   \
  X(ERRC_NO_RESPONSE, "NO_RESPONSE", "No response")                                                                                                                                                    \
  X(ERRC_OVER_MAX, "OVER_MAX", "Value over maximum")                                                                                                                                                   \
  X(ERRC_UNDER_MIN, "UNDER_MIN", "Value under minimum")                                                                                                                                                \
  X(ERRC_TIMEOUT, "TIMEOUT", "Operation timeout")                                                                                                                                                      \
  X(ERRC_MODE_SWITCH_FAIL, "MODE_SWITCH_FAILED", "Mode switch failed")                                                                                                                                 \
  X(ERRC_AUTOTUNE_FAIL, "AUTOTUNE_FAILED", "PID autotune failed")                                                                                                                                      \
  X(ERRC_CONFIG_INVALID, "CONFIG_INVALID", "Invalid configuration")                                                                                                                                    \
  X(ERRC_STATE_CHANGE, "ERRC_STATE_CHANGE", "Mode state change")                                                                                                                                       \
  X(ERRC_PROTOCOL_VERSION, "PROTOCOL_VERSION", "Protocol version mismatch")

/**
 X(ERRC_OK,                     "OK",                        "OK") \
 X(ERRC_SENSOR_HEATER_INVALID,  "HEATER_SENSOR_INVALID",     "Thermistor invalid") \
 X(ERRC_SENSOR_HEATER_SHORT,    "SENSOR_HEATER_SHORT",       "Thermistor short circuit") \
 X(ERRC_SENSOR_HEATER_OPEN,     "SENSOR_HEATER_OPEN",        "Thermistor open circuit") \
 X(ERRC_SENSOR_AIR_INVALID,     "AIR_SENSOR_INVALID",        "Air sensor invalid") \
 X(ERRC_HEATER_NO_RESPONSE,     "HEATER_NO_RESPONSE",        "Heater no response") \
 X(ERRC_AIR_NO_RESPONSE,        "AIR_NO_RESPONSE",           "Air no response") \
 X(ERRC_HEATER_OVERMAX,         "HEATER_OVERMAX",            "Heater over maximum") \
 X(ERRC_AIR_OVERMAX,            "AIR_OVERMAX",               "Air over maximum") \
 X(ERRC_MODE_SWITCH_FAILED,     "MODE_SWITCH_FAILED",        "Mode switch failed") \
 X(ERRC_STORAGE_HUM_TIMEOUT,    "STORAGE_HUM_TIMEOUT",       "Storage RH timeout")
 */
