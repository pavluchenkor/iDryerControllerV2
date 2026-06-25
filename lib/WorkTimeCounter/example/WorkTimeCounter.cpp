#include "WorkTimeCounter.h"

WorkTimeCounter counter(0);

void setup() {
    counter.begin();
    counter.start(); // Запускаем подсчёт времени
}

void loop() {
    counter.tick(); // вызывать часто
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 5000) {
        lastPrint = millis();
        Serial.printf("Часы: %lu, Минуты: %u\n",
                      counter.getHours(),
                      counter.getMinutes());
    }
}