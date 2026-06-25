#include "sensor/ThermistorSensor.h"
#include <math.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_THERMISTOR
#define LOG_TAG "THERMISTOR"
#include "debug_log.h"

ThermistorSensor::ThermistorSensor(uint8_t adc_pin,
                                   const ThermistorData *table,
                                   float pullup_ohm, float inline_res_ohm,
                                   float ema_alpha, float minC, float maxC)
    : pin_(adc_pin), table_(table), pullup_(pullup_ohm),
      inline_r_(inline_res_ohm), minC_(minC), maxC_(maxC) {
  ema_filter_init(&ema_, ema_alpha);
}

void ThermistorSensor::setRange(float minC, float maxC) {
  minC_ = minC;
  maxC_ = maxC;
}

void ThermistorSensor::setRawFaultThresholds(int low_short, int high_open) {
  raw_short_thr_ = low_short;
  raw_open_thr_ = high_open;
}

ThermistorSensor ThermistorSensor::byType(uint8_t adc_pin, uint8_t type_id,
                                          float pullup_ohm, float inline_res_ohm,
                                          float ema_alpha) {
  return ThermistorSensor(adc_pin, get_thermistor_data(type_id), pullup_ohm,
                          inline_res_ohm, ema_alpha);
}

void ThermistorSensor::setType(uint8_t type_id) {
  table_ = get_thermistor_data(type_id);
  if (table_) {
    thermistor_set_coefficients(&th_, table_->t1, table_->r1, table_->t2,
                                table_->r2, table_->t3, table_->r3);
  }
}

bool ThermistorSensor::begin() {
  thermistor_init(&th_, pullup_, inline_r_);
  if (table_) {
    thermistor_set_coefficients(&th_, table_->t1, table_->r1, table_->t2,
                                table_->r2, table_->t3, table_->r3);
  }

#if defined(ARDUINO_ARCH_RP2040)
  analogReadResolution(12);
#endif
  pinMode(pin_, INPUT);
  last_ = {};

  unsigned long now_ms = millis();

  int raw = 0;
  for (int i = 0; i < 10; i++) {
    raw += analogRead(pin_);
    delay(10);
  }
  raw /= 10;

  DEBUG_T("ADC RAW: %d", raw);
  if (raw <= raw_short_thr_) {
    fillError(now_ms, ThermErr::ShortCircuit);
    return false;
  }
  if (raw >= raw_open_thr_) {
    fillError(now_ms, ThermErr::OpenCircuit);
    return false;
  }
  if (!table_) {
    fillError(now_ms, ThermErr::NoTable);
    return false;
  }
  return true;
}

void ThermistorSensor::tick(uint32_t now_ms) {
  const float period_ms = 1000.0f / (rate_hz_ > 0 ? rate_hz_ : 1.0f);
  if ((now_ms - last_tick_ms_) < period_ms)
    return;
  last_tick_ms_ = now_ms;

  // --- чтение АЦП и нормализация ---
#if defined(ARDUINO_ARCH_RP2040)
  constexpr int ADC_BIT = 12;
  constexpr float ADC_MAX = 4095.0f;
  analogReadResolution(12);
#else
  constexpr int ADC_BIT = 10;
  constexpr float ADC_MAX = 1023.0f;
#endif
  const int raw = analogRead(pin_);

  // --- быстрый детект "железных" отказов до фильтрации ---
  if (raw <= raw_short_thr_) {
    fillError(now_ms, ThermErr::ShortCircuit);
    return;
  }
  if (raw >= raw_open_thr_) {
    fillError(now_ms, ThermErr::OpenCircuit);
    return;
  }
  if (!table_) {
    fillError(now_ms, ThermErr::NoTable);
    return;
  }

  const float norm =
      (raw <= 0) ? 0.0f : (raw >= ADC_MAX ? 1.0f : (float)raw / ADC_MAX);

  // --- сглаживание ---
  const float filt = ema_filter_update(&ema_, norm);

  // --- температура по фильтрованной норме ---
  const float tC = thermistor_calc_temp(&th_, filt);

  // --- проверки диапазона ---
  bool ok = !isnan(tC);
  ThermErr err = ThermErr::OK;
  if (!ok) {
    err = ThermErr::OutOfRange;
  } else if (tC < minC_ || tC > maxC_) {
    ok = false;
    err = ThermErr::OutOfRange;
  }
  DEBUG_I("ADC RAW: %d   tC:%.1f", raw, (double)tC);

  last_.temperature = tC;
  last_.humidity = NAN;
  last_.pressure = NAN;
  last_.ts_ms = now_ms;
  last_.ok = ok;
  last_.err = static_cast<int>(err);
}

void ThermistorSensor::fillError(uint32_t now_ms, ThermErr e) {
  last_.temperature = NAN;
  last_.ts_ms = now_ms;
  last_.ok = false;
  last_.err = static_cast<int>(e);
}
