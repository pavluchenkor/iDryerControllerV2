// Sensor.h
#pragma once
#include <stdint.h>
#include <Arduino.h>
#include <math.h>   // для NAN

struct SensorReading {
  float    temperature = NAN;
  float    humidity    = NAN;
  float    pressure    = NAN;
  uint32_t ts_ms       = 0;     // когда обновили показания
  bool     ok          = false; // валидны ли данные
  int      err         = 0;     // код ошибки (0 = ок)
};

// Утилита: "протухли" ли показания r к моменту now_ms?
inline bool isStale(const SensorReading& r,
                    uint32_t now_ms,
                    uint32_t stale_ms =1000) {
  return !r.ok || (now_ms - r.ts_ms) > stale_ms;
}

class ISensor {
public:
  virtual ~ISensor() = default;

  virtual bool begin() = 0;                 // инициализация железа/шины
  virtual void setRateHz(float hz) { rate_hz_ = hz; }
  virtual void tick(uint32_t now_ms) = 0;   // опрос/стейт-машина
  virtual SensorReading get() const = 0;    // последнее значение

  static constexpr uint32_t STALE_MS = 2000;

  // Удобная обёртка: проверить "протухание" текущего показания датчика
  bool isStale(uint32_t now_ms) const {
    return ::isStale(get(), now_ms, STALE_MS);
  }

protected:
  float rate_hz_ = 1.0f;  // частота опроса по умолчанию
};