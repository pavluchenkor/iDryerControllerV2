#pragma once
#include <Arduino.h>
#include <stdint.h>

#pragma once
#include <stdint.h>

#if defined(ARDUINO_ARCH_RP2040)
  #include <hardware/sync.h>   // critical_section_t
  #include <pico/sync.h>
#endif

class SoftPwmManager {
public:
  static constexpr int kMaxChannels = 16; // подстрой под RP2040_Slow_PWM

  static SoftPwmManager& instance();

  // Легаси-API
  int  begin(uint8_t pin, float freq_hz, float duty_pct);

  // Современное/безопасное
  int  addChannel(uint8_t pin, float freq_hz, float duty_pct);
  void setDuty(int ch, float duty_pct);
  void setFreq(int ch, float freq_hz);
  void removeChannel(int ch);
  bool beginBackend(uint32_t tick_us);

  // Отладка/проверки
  bool    valid(int ch) const;
  uint8_t pinOf(int ch) const;

private:
  struct Slot {
    bool     used;
    uint8_t  pin;
    float    freq;
    float    duty;
    int      libCh;
  };

  Slot slots_[kMaxChannels];
  int  slotOfLibCh_[kMaxChannels];
  unsigned long backendTickUs_;

#if defined(ARDUINO_ARCH_RP2040)
  mutable critical_section_t cs_;
  bool backendStarted_ = false;
#endif

  int  findSlotByPin(uint8_t pin) const;
  int  allocFreeSlot();

  SoftPwmManager();
  SoftPwmManager(const SoftPwmManager&) = delete;
  SoftPwmManager& operator=(const SoftPwmManager&) = delete;
};

// class SoftPwmManager {
// public:
//   // static constexpr int kMaxChannels = 16;
//   // bool  chUsed_[kMaxChannels] = {};
//   // float chFreq_[kMaxChannels] = {};
  
  
//   static SoftPwmManager& instance();
  
//   bool begin(uint32_t timer_interval_us = 20);
//   int addChannel(uint32_t pin, float freq_hz, float duty_percent0);
//   void setDuty(int channel, float duty_percent);
//   void removeChannel(int channel);
  
//   private:
//   SoftPwmManager() = default;

//   // Храним параметры каналов, чтобы уметь дергать 3-аргументный setPWM(channel, freq, duty)
//   static constexpr int kMaxChannels = 16; // тест: лимит каналов RP2040_Slow_PWM
//   float chFreq_[kMaxChannels] = {0};      // тест: частота каждого канала (Гц)
//   bool  chUsed_[kMaxChannels] = {false};  // тест: пометка, что канал выделен
//   int   chPin_[kMaxChannels]  = {};       // сохраняем id канала из RP2040_Slow_PWM
//   bool  started_ = false;
// };
