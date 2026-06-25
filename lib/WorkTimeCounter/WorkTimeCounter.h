#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include <stdint.h>
#include "menu/menu_eeprom.h"
#include "error/error_eelog.h"

// -----------------------------------------------------------------------------
// СЧЁТЧИК НАРАБОТКИ, НЕ ЗАВИСИТ ОТ ДЛИНЫ МЕНЮ
// Пишем ЧАСЫ/МИНУТЫ по макросам из menu_eeprom.h:
// EE_OFF_WT_HOURS / EE_OFF_WT_MINUTES
// Если их нет (старый header) - падаем на дефолты 0x08 / 0x0C.
// -----------------------------------------------------------------------------

#ifndef EE_OFF_WT_HOURS
  #define EE_OFF_WT_HOURS   0x08
#endif
#ifndef EE_OFF_WT_MINUTES
  #define EE_OFF_WT_MINUTES 0x0C
#endif

class WorkTimeCounter {
public:
  WorkTimeCounter() = default;

  // Если EEPROM.begin(size) НЕ вызывался ранее
  void begin(size_t eepromSize) {
    EEPROM.begin(eepromSize);
    load_();
    lastTickMs_      = millis();
    lastSavedMinutes_= minutes_;
    lastSaveMs_      = lastTickMs_;
  }

  // Если EEPROM.begin(size) уже был
  void beginNoInit() {
    load_();
    lastTickMs_      = millis();
    lastSavedMinutes_= minutes_;
    lastSaveMs_      = lastTickMs_;
  }

  void start() { running_ = true;  lastTickMs_ = millis(); }

  void stop()  {
    tick();                 // дотянем накопившееся
    running_ = false;
    save_();                // форс-сохранение при остановке
    lastSavedMinutes_ = minutes_;
    lastSaveMs_ = millis();
  }

  // Обновление таймера (вызывать часто)
  void tick() {
    #define minute2ms 60000u // для ускоренного теста (60 000u)
    if (!running_) return;
    const uint32_t now = millis();
    uint32_t dt = now - lastTickMs_;
    if (dt >= minute2ms) {                 //60000 +1 минута за каждую полную минуту
      const uint32_t addMin = dt / minute2ms;
      lastTickMs_ += addMin * minute2ms;
      addMinutes_(addMin);

      // Автосохранение: если минута изменилась и прошёл минимум по времени - пишем
      if (minutes_ != lastSavedMinutes_) {
        save_();
        lastSavedMinutes_ = minutes_;
        lastSaveMs_ = now;
      }
    }
  }

  // Принудительное сохранение (например, на приветственном экране)
  void saveNow() { save_(); lastSavedMinutes_ = minutes_; lastSaveMs_ = millis(); }

  // Сброс + немедленное сохранение
  void reset()   { hours_ = 0; minutes_ = 0; save_(); lastSavedMinutes_ = minutes_; lastSaveMs_ = millis(); }

  uint32_t getHours()  const { return hours_; }
  uint8_t  getMinutes()const { return minutes_; }

private:
  uint32_t hours_   = 0;
  uint8_t  minutes_ = 0;   // 0..59
  bool     running_ = false;
  uint32_t lastTickMs_ = 0;

  // Политика сохранения
  uint8_t  lastSavedMinutes_ = 0;
  uint32_t lastSaveMs_       = 0;
  // uint32_t saveMinIntervalMs_= 10000u; // 10 секунд между автосейвами

  static constexpr uint16_t OFF_H_ = EE_OFF_WT_HOURS;
  static constexpr uint16_t OFF_M_ = EE_OFF_WT_MINUTES;

  void addMinutes_(uint32_t m) {
    const uint32_t total = uint32_t(minutes_) + m;
    hours_  += total / 60u;
    minutes_ = uint8_t(total % 60u);
  }

  void load_() {
    uint32_t h32 = 0, m32 = 0;
    EEPROM.get(OFF_H_, h32); // читаем 4 байта
    EEPROM.get(OFF_M_, m32); // читаем 4 байта

    bool bad = false;
    if (h32 == 0xFFFFFFFFu) { h32 = 0; bad = true; }
    if (m32 == 0xFFFFFFFFu || m32 > 59u) { m32 = 0; bad = true;}

    hours_ = h32;
    minutes_ = (uint8_t)m32;

    // if (bad) { // если было мусорное значение сразу нормализуем в EEPROM
    //   save_();
    // }
  }

  void save_() {
    EEPROM.put(OFF_H_, hours_);

    // пишем 4 байта, чтобы байты [13..15] не оставались 0xFF
    uint32_t m32 = minutes_;
    EEPROM.put(OFF_M_, m32);

#if defined(EEPROM_CLASS_VERSION) || defined(ARDUINO_ARCH_RP2040)
    EEPROM.commit();
    // sys_shadow_check("wtc_save");
#endif
  }
};