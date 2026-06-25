#pragma once
#include "ICsPin.h"
#include <Arduino.h>

class GpioCs : public ICsPin {
public:
    explicit GpioCs(uint8_t pin) : _pin(pin) {}

    void init()     override { pinMode(_pin, OUTPUT); digitalWrite(_pin, HIGH); }
    void select()   override { digitalWrite(_pin, LOW); }
    void deselect() override { digitalWrite(_pin, HIGH); }

    uint8_t pin() const { return _pin; }  // для передачи в PN532_SPI

private:
    uint8_t _pin;
};
