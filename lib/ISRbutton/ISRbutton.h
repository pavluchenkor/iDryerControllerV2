#pragma once
#include <Arduino.h>

class ISRbutton {
public:
    ISRbutton(uint8_t pin,
              uint8_t pinModeCfg = INPUT_PULLUP,
              PinStatus interruptMode = CHANGE,
              uint8_t activeLevel = LOW,
              uint32_t holdMs = 700,
              uint32_t debounceUs = 5000);

    void init(uint8_t pin,
              uint8_t pinModeCfg,
              PinStatus interruptMode,
              uint8_t activeLevel,
              uint32_t holdMs,
              uint32_t debounceUs);

    void setHoldThreshold(uint16_t ms);
    void setDebounceUs(uint32_t us);

    void tick();        // должен вызываться в loop()

    bool pressed();     // true один раз при нажатии
    bool released();    // true один раз при отпускании (после hold не вернётся)
    bool held();        // true один раз за удержание

    bool read() const { return state_; } // текущее состояние

private:
    // Статическая обёртка для attachInterrupt
    static void isrTrampoline();
    void handleISR();

    static ISRbutton* instance_;

    // Конфигурация
    uint8_t  pin_;
    uint8_t  actLevel_;
    uint32_t holdThresholdUs_;
    uint32_t debounceUs_;

    // Состояние
    volatile bool state_;
    volatile bool evtPress_;
    volatile bool evtRelease_;
    volatile bool evtHold_;
    volatile bool holdArmed_;
    volatile bool holdReported_;
    volatile bool suppressRel_;

    volatile uint32_t tPress_;

    // Отложенный дебаунс
    volatile bool     pendEdge_;
    volatile bool     pendLevel_;
    volatile uint32_t tApplyUs_;
};




// #pragma once
// #include <Arduino.h>

// class ISRbutton {
// public:
//   ISRbutton(uint8_t pin,
//             uint8_t pinModeCfg = INPUT_PULLUP,
//             PinStatus interruptMode = CHANGE,
//             uint8_t activeLevel = LOW,
//             uint32_t holdMs = 500,
//             uint32_t debounceUs = 10000);

//   void init(uint8_t pin,
//             uint8_t pinModeCfg,
//             PinStatus interruptMode,
//             uint8_t activeLevel,
//             uint32_t holdMs,
//             uint32_t debounceUs);

//   void setHoldThreshold(uint16_t ms);
//   void setDebounceUs(uint32_t us);

//   void tick();
//   bool pressed();
//   bool released();
//   bool held();

// private:
//   static ISRbutton* instance_;
//   static void isrTrampoline();
//   void handleISR();

//   uint8_t  pin_;
//   uint8_t  actLevel_;
//   uint32_t holdThresholdUs_;
//   uint32_t debounceUs_;

//   volatile bool state_;
//   volatile bool evtPress_;
//   volatile bool evtRelease_;
//   volatile bool evtHold_;
//   volatile bool holdArmed_;
//   volatile bool holdReported_;
//   volatile bool suppressRel_;

//   volatile uint32_t tLastEdge_;
//   volatile uint32_t tPress_;
// };