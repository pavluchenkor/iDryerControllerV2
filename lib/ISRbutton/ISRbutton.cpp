#include "ISRbutton.h"

ISRbutton *ISRbutton::instance_ = nullptr;

ISRbutton::ISRbutton(uint8_t pin, uint8_t pinModeCfg, PinStatus interruptMode, uint8_t activeLevel, uint32_t holdMs, uint32_t debounceUs) {
  init(pin, pinModeCfg, interruptMode, activeLevel, holdMs, debounceUs);
}

void ISRbutton::init(uint8_t pin, uint8_t pinModeCfg, PinStatus interruptMode, uint8_t activeLevel, uint32_t holdMs, uint32_t debounceUs) {
  pin_ = pin;
  actLevel_ = activeLevel;
  holdThresholdUs_ = (uint32_t)holdMs * 1000UL;
  debounceUs_ = debounceUs;

  pinMode(pin_, pinModeCfg);

  state_ = (digitalRead(pin_) == actLevel_);
  evtPress_ = false;
  evtRelease_ = false;
  evtHold_ = false;
  holdArmed_ = false;
  holdReported_ = false;
  suppressRel_ = false;
  tPress_ = 0;

  pendEdge_ = false;
  pendLevel_ = false;
  tApplyUs_ = 0;

  instance_ = this;
  attachInterrupt(digitalPinToInterrupt(pin_), ISRbutton::isrTrampoline, interruptMode);
}

void ISRbutton::setHoldThreshold(uint16_t ms) { holdThresholdUs_ = (uint32_t)ms * 1000UL; }

void ISRbutton::setDebounceUs(uint32_t us) { debounceUs_ = us; }

void ISRbutton::tick() {
  uint32_t now = micros();

  // 1) Применяем отложенный фронт
  if (pendEdge_ && (int32_t)(now - tApplyUs_) >= 0) {
    bool levelActiveNow = (digitalRead(pin_) == actLevel_);

    if (levelActiveNow == pendLevel_) {
      if (levelActiveNow && !state_) {
        // НАЖАТИЕ
        noInterrupts();
        state_ = true;
        evtPress_ = true;
        evtRelease_ = false;
        holdArmed_ = false;
        holdReported_ = false;
        suppressRel_ = false;
        evtHold_ = false;
        tPress_ = now;
        interrupts();
      } else if (!levelActiveNow && state_) {
        // ОТПУСКАНИЕ
        noInterrupts();
        state_ = false;
        evtRelease_ = true;
        holdArmed_ = false;
        interrupts();
      }
    }
    pendEdge_ = false; // сброс кандидата
  }

  // 2) Проверка удержания
  if (state_ && !holdArmed_) {
    if ((uint32_t)(now - tPress_) >= holdThresholdUs_) {
      noInterrupts();
      evtHold_ = true;
      holdArmed_ = true;
      holdReported_ = false;
      suppressRel_ = true; // подавим release после hold
      interrupts();
    }
  }
}

bool ISRbutton::pressed() {
  noInterrupts();
  bool v = evtPress_;
  evtPress_ = false;
  interrupts();
  return v;
}

bool ISRbutton::released() {
  noInterrupts();
  bool v = evtRelease_;
  if (v && suppressRel_) {
    v = false;
    suppressRel_ = false;
  }
  evtRelease_ = false;
  interrupts();
  return v;
}

bool ISRbutton::held() {
  noInterrupts();
  bool fire = (evtHold_ && !holdReported_);
  if (fire) holdReported_ = true;
  interrupts();
  return fire;
}

void ISRbutton::isrTrampoline() {
  if (instance_) instance_->handleISR();
}

void ISRbutton::handleISR() {
  uint32_t now = micros();
  pendLevel_ = (digitalRead(pin_) == actLevel_);
  tApplyUs_ = now + debounceUs_;
  pendEdge_ = true;
}
