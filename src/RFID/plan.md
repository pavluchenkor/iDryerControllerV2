# RFID — план реализации

## Решения зафиксированные

- Чип: PN532, SPI1, пины: MISO=12, SCK=14, MOSI=15, RST=18
- CS0=NSS=19 (reader 0), CS1=BUSY=13 (reader 1)
- CS-абстракция: `ICsPin` → `GpioCs` сейчас, `ShiftRegCs` (74HC595) в будущем
- Чип-абстракция: `IRfidDriver` → `Pn532Driver` сейчас, `Pn5180Driver` (ISO 15693) в будущем
- `RfidManager` работает только с `IRfidDriver*` — не знает про конкретный чип
- Polling в loop() — IRQ не используется (elechouse PN532_SPI не требует)
- Один SPI-экземпляр (SPI1), два драйвера с разными CS
- 888 байт payload (NTAG216) — размер на стороне бэкенда, не прошивки
- Привязка reader → unit через конфиг (аналогично весам)
- LED-дыхание: индикация при появлении/уходе известной и неизвестной метки
- Детальный план по умному опросу, весовым критериям и почти `interrupt-style` схеме: `src/RFID/plan-smart-polling-and-tag-sync.md`
- Детальный план перехода на `PN5180` и причины, почему `PN532` остается только промежуточным этапом: `src/RFID/PN5180_plan.md`

---

## Этап 1 — Железо: запуск, чтение, тестовая запись

- [x] Удалить `rfid_service.h` и `rfid_service.cpp` (мусор MFRC522/I2C)
- [x] Создать `ICsPin.h` — интерфейс CS-пина
- [x] Создать `GpioCs.h` — прямой GPIO, реализация ICsPin
- [x] Создать `IRfidDriver.h` — абстрактный интерфейс драйвера
- [x] Создать `drivers/Pn532Driver.h` и `.cpp` — реализация под PN532, принимает GpioCs&
- [x] Создать `RfidManager.h` и `.cpp` — массив драйверов, isAvailable(), poll(), read(), write()
- [x] Подключить в `hardware.cpp`: создать GpioCs, Pn532Driver, RfidManager (аналогично HX711)
- [x] Добавить `rfidManager.begin()` в `initHardware()`
- [x] Polling в `serviceTasks()`: Serial-вывод UID при появлении/уходе метки (~200мс)
- [ ] Тестовая запись: несколько байт на NTAG, верификация чтением

## Этап 2 — UART-интеграция

- [x] При появлении метки → отправить `Rfid (0x14)` через uartBridge (TagDetected + UID + readerId + unitId)
- [x] При уходе метки → отправить `Rfid (0x14)` (TagRemoved, с отложенной отправкой если катушка ещё на весах)
- [ ] Перенести обработчики `ReadRfid`/`WriteRfid` из `main.cpp` в отдельный модуль (рядом с RfidManager)
- [ ] Команда `ReadRfid (0x07)`: прочитать 888 байт с метки → отправить `RfidReadData (0x1A)` шестью фрагментами по 163 байт
- [ ] Команда `WriteRfid (0x08)`: принять 6 фрагментов `RfidWriteData (0x1B)` → собрать буфер → записать на метку → ACK

## Этап 3 — LED-индикация

- [x] TagDetected / TagRemoved → фиолетовое дыхание 3 раза (ledsShowWebBreath PURPLE 1000мс/3с)
- [ ] Известная метка (UID в HelloPayload) → дыхание цвет A
- [ ] Неизвестная метка → дыхание цвет B
- [ ] Запись идёт → дыхание цвет C
- [ ] Запись успешна / ошибка → вспышка

## Этап 4 — Портал (edge cases)

- [ ] MIFARE Classic зашифрован (Bambu) → событие с флагом, не падать
- [ ] Метка ушла в момент записи → `WRITE_ACK { status: error, code: tag_lost }`
- [ ] Два ридера одновременно: обработка обоих UID в одном poll-цикле

## Этап 5 — Будущее (не сейчас)

- [ ] `ShiftRegCs.h` — реализация ICsPin через 74HC595 (3→8 CS линий)
- [ ] 74HC139 на плате (rev 3): 4 ридера через A0/A1
- [ ] `Pn5180Driver` — поддержка ISO 15693 / OpenPrintTag
