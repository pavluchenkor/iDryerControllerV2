// sensor/Sht31Sensor.h
#pragma once
#include "sensor/Sensor.h"
#include <Arduino.h>
#include <Wire.h>
#include <SHT31.h>
#include "error/error_post.h"      // post_err_ex, sensor_err_code_for
#include "error/error_defs.h"      // ErrSource/ErrCode/ERRSEV_*
#include "sensor/ThermistorSensor.h" // ThermErr enum для heater

/** Хелпер для печати ошибок **/
static const char* shtErrToStr(int e) {
  switch (e) {
    case 0:  return "OK";
    case 1:  return "NotFound";
    case 2:  return "NoData";
    case 3:  return "ReadFail";
    case 4:  return "OutOfRange";
    default: return "Unknown";
  }
}

/** Коды ошибок SHT31 **/
enum class ShtErr : uint8_t {
  OK          = 0,
  NotFound    = 1,  // begin() не увидел чип
  NoData      = 2,  // dataReady() = false слишком долго
  ReadFail    = 3,  // readData() = false
  OutOfRange  = 4   // температура или влажность вне диапазона
};

class Sht31Sensor : public ISensor {
public:
  Sht31Sensor(uint8_t addr, TwoWire* bus, 
              float minC=-40, float maxC=125, // max. values for T=-40°C … 125°C https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf
              float minRH=0, float maxRH=100)
  : bus_(bus), dev_(addr, bus), 
    minC_(minC), maxC_(maxC), minRH_(minRH), maxRH_(maxRH) {}

  bool begin() override {
    if (!bus_)         {fillError(millis(), ShtErr::NotFound); return false; }  // если шина не передана
    if (!dev_.begin()) {fillError(millis(), ShtErr::NotFound); return false; } // если датчик не откликнулся
    last_ = {};
    dev_.requestData();                    // запустить первое измерение
    return true;                           // всё ок
  }

  void tick(uint32_t now_ms) override {
    const float period_ms = 1000.0f / (rate_hz_ > 0 ? rate_hz_ : 1.0f);
    if ((now_ms - prev_ms_) < period_ms) return;
    prev_ms_ = now_ms;

    if (!dev_.dataReady()) { fillError(now_ms, ShtErr::NoData); return;}
    if (!dev_.readData())  { fillError(now_ms, ShtErr::ReadFail);dev_.requestData(); return;}//повторить запрос

    // прочитано успешно
    float tC = dev_.getTemperature();
    float h  = dev_.getHumidity();

    bool ok = true;
    ShtErr err = ShtErr::OK;

    if (isnan(tC) || tC < minC_ || tC > maxC_) {ok = false; err = ShtErr::OutOfRange;}
    if (isnan(h) || h < minRH_ || h > maxRH_)  {ok = false; err = ShtErr::OutOfRange;}

    float a = 0.7f; //TODO: finish the crutch ema
    last_.temperature = last_.temperature > 0.0f ? last_.temperature + a * (tC - last_.temperature) : 1.0f;
    last_.humidity    = last_.humidity > 0.0f ? last_.humidity + a * (h - last_.humidity) : 0.1f;
    // last_.temperature = tC;
    // last_.humidity    = h;
    last_.pressure    = NAN;
    last_.ts_ms       = now_ms;
    last_.ok          = ok;
    last_.err         = static_cast<int>(err);

    dev_.requestData(); // новый цикл
  }

  SensorReading get() const override { return last_; }

private:
  void fillError(uint32_t now_ms, ShtErr e) {
    last_.temperature = NAN;
    last_.humidity    = NAN;
    last_.pressure    = NAN;
    last_.ts_ms       = now_ms;
    last_.ok          = false;
    last_.err         = static_cast<int>(e);
  }

  TwoWire* bus_;
  SHT31    dev_;
  float    minC_, maxC_;
  float    minRH_, maxRH_;
  uint32_t prev_ms_ = 0;
  mutable SensorReading last_;
};