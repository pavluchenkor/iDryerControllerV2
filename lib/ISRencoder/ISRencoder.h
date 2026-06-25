// ISRencoder.h
#pragma once
#include <Arduino.h>

class ISRencoder {
public:
    // +1 = вправо, -1 = влево, 0 = нет движения; накапливает шаги
    volatile int16_t delta = 0;

    ISRencoder(uint8_t encA, uint8_t encB, uint8_t mode = INPUT_PULLUP, PinStatus interruptMode = CHANGE);
    void init(uint8_t encA, uint8_t encB, uint8_t mode = INPUT_PULLUP, PinStatus interruptMode = CHANGE);

    // Забрать и обнулить накопленное (потокобезопасно)
    int16_t tick();

    // Посмотреть текущее накопленное без сброса
    int16_t tickRaw() const;

    volatile int8_t stepAccum = 0;             // накопление микрошагов
    volatile int16_t lastStepValue = 0;        // значение последнего полного шага
    volatile uint32_t lastStepTime = 0;        // время последнего полного шага
    uint32_t accelThreshold = 40000;            // 5 мс

private:
    uint8_t e0 = 0, e1 = 0;           // пины A/B
    volatile uint8_t last = 0;        // последние биты состояния AB

    // Таблица переходов квадратиуры: возвращает -1/0/+1
    static int8_t stepFrom(uint8_t last, uint8_t now);

    // Общий обработчик для обоих пинов
    void handleISR();

    // Трамплин для attachInterrupt
    static void isrTrampoline();

    // Поддержка одного инстанса
    static ISRencoder* instance;

};