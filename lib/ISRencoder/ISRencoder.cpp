// ISRencoder.cpp
#include "ISRencoder.h"

ISRencoder* ISRencoder::instance = nullptr;

ISRencoder::ISRencoder(uint8_t encA, uint8_t encB, uint8_t mode, PinStatus interruptMode) {
    init(encA, encB, mode, interruptMode);
}

void ISRencoder::init(uint8_t encA, uint8_t encB, uint8_t mode, PinStatus interruptMode) {
    e0 = encA;
    e1 = encB;

    pinMode(e0, mode);
    pinMode(e1, mode);

    // начальное состояние (AB) в 2 битах
    last = (digitalRead(e0) ? 1 : 0) | (digitalRead(e1) ? 2 : 0);

    instance = this;

    attachInterrupt(digitalPinToInterrupt(e0), &ISRencoder::isrTrampoline, interruptMode);
    attachInterrupt(digitalPinToInterrupt(e1), &ISRencoder::isrTrampoline, interruptMode);
}

int16_t ISRencoder::tick() {
    noInterrupts();
    int16_t val = lastStepValue;
    lastStepValue = 0; // выдаём ровно один раз
    interrupts();
    return val;
}

int16_t ISRencoder::tickRaw() const {
    return delta;
}

int8_t ISRencoder::stepFrom(uint8_t last, uint8_t now) {
    static const int8_t lut[16] = {
        /*0000*/ 0, /*0001*/ +1, /*0010*/ -1, /*0011*/ 0,
        /*0100*/ -1,/*0101*/ 0,  /*0110*/  0, /*0111*/ +1,
        /*1000*/ +1,/*1001*/ 0,  /*1010*/  0, /*1011*/ -1,
        /*1100*/ 0, /*1101*/ -1, /*1110*/ +1, /*1111*/ 0
    };
    return lut[(last << 2) | now];
}



void ISRencoder::handleISR() {
    uint8_t now = (digitalRead(e0) ? 1 : 0) | (digitalRead(e1) ? 2 : 0);
    int8_t s = stepFrom(last, now);
    last = now;

    if (s) {
        stepAccum += s;

        // полный шаг = 4 микрошагa
        if (abs(stepAccum) >= 4) {
            uint32_t t = micros();
            uint32_t dt = t - lastStepTime;
            lastStepTime = t;

            int8_t stepValue = (dt < accelThreshold) ? 10 : 1;
            lastStepValue = (stepAccum > 0) ? stepValue : -stepValue;

            stepAccum = 0; // сброс накопителя
        }
    }
}


void ISRencoder::isrTrampoline() {
    if (instance) instance->handleISR();
}