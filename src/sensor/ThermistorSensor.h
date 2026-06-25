#pragma once
#include "sensor/Sensor.h"
#include <Arduino.h>

extern "C" {
#include "ema_filter.h"      // EmaFilter, ema_filter_init, ema_filter_update
#include "thermistor.h"      // Thermistor, thermistor_init, thermistor_set_coefficients, thermistor_calc_temp
#include "thermistor_data.h" // ThermistorData, get_thermistor_data(type_id)
}

// --- удобные коды ошибок по термистору
enum class ThermErr : uint8_t {
  OK = 0,
  OutOfRange = 1,   // tC < minC_ || tC > maxC_
  OpenCircuit = 2,  // обрыв (ADC ≈ Vref)
  ShortCircuit = 3, // КЗ (ADC ≈ 0)
  NoTable = 4       // нет коэффициентов
};

class ThermistorSensor : public ISensor {
public:
  ThermistorSensor(uint8_t adc_pin, const ThermistorData *table,
                   float pullup_ohm, float inline_res_ohm, float ema_alpha,
                   float minC = -40.0f, float maxC = 160.0f);

  void setRange(float minC, float maxC);
  void setRawFaultThresholds(int low_short, int high_open);
  static ThermistorSensor byType(uint8_t adc_pin, uint8_t type_id,
                                 float pullup_ohm, float inline_res_ohm,
                                 float ema_alpha);
  void setType(uint8_t type_id);

  bool begin() override;
  void tick(uint32_t now_ms) override;
  SensorReading get() const override { return last_; }

private:
  void fillError(uint32_t now_ms, ThermErr e);

  uint8_t pin_;
  const ThermistorData *table_ = nullptr;
  float pullup_ = 10000.0f;
  float inline_r_ = 0.0f;
  float minC_ = -40.0f;     // минимальная температура
  float maxC_ = 160.0f;     // максимальная температура
  int raw_short_thr_ = 40;  // порог КЗ (АЦП)
  int raw_open_thr_ = 4040; // порог обрыва (АЦП) 3754  4090

  Thermistor th_{};
  EmaFilter ema_{};

  mutable SensorReading last_;
  uint32_t last_tick_ms_ = 0;
};
