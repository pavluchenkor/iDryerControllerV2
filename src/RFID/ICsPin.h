#pragma once
#include <stdint.h>

/**
 * Абстракция CS-пина для SPI-устройств.
 *
 * Текущая реализация: GpioCs (прямой GPIO).
 * Будущая реализация: ShiftRegCs (74HC595 через SPI).
 *
 * При переходе на сдвиговый регистр:
 *   1. Реализовать ShiftRegCs : ICsPin
 *   2. Создать кастомный PN532Interface, использующий ICsPin вместо digitalWrite
 *   3. Заменить GpioCs на ShiftRegCs в hardware.cpp
 */
class ICsPin {
public:
    virtual ~ICsPin() = default;
    virtual void init()     = 0;  // pinMode + начальное состояние HIGH
    virtual void select()   = 0;  // CS → LOW
    virtual void deselect() = 0;  // CS → HIGH
};
