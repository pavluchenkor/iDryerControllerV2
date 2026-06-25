# Стратегия записи RFID-меток в iDryer

Тезисы по архитектуре: какой формат на какой чип, кто решает, кто пишет.

---

## OpenPrintTag (Prusa) — приоритетная цель

- Stock NFC-антенны в **MK4S / MK3.9S / Core One** сейчас **пассивные** (для телефонов, не катушек)
- **Активная поддержка авточтения катушек у Prusa — Q1 2026** (либо уже появилась, либо вот-вот)
- Чип — **ICODE SLIX2 (ISO 15693)**
- **PN5180 умеет** (есть рабочий коммит: `✅ NTAG213/215 + SLIX (ISO15693) запись/чтение работает`)
- Кодирование: **NDEF + CBOR**, MIME `application/vnd.openprinttag`, ~316 байт пользовательских данных
- На SLIX2 → пишем **только OpenPrintTag**, формат там один (Prusa-only экосистема)
- Технически препятствий нет → когда Prusa включит активный ридер, катушки заработают сразу

---

## Klipper — без записи на метку

- Подход: портал как оракул, метка = только UID
- Флоу: сушилка читает UID → портал смотрит свою БД / Spoolman → пушит данные в **Moonraker API** → Klipper подхватывает в макросах
- **Метку не пишем вообще**

---

## Bambu — без записи на метку

- Push в **MQTT** (как делает OpenSpool)
- Метку не трогаем (всё равно RSA-подпись Bambu подделать нельзя)

---

## Итоговая матрица

| Целевой принтер | Что делает портал | Запись на метку |
|---|---|---|
| Bambu | MQTT push | ❌ |
| Klipper (Moonraker) | API push в Moonraker / Spoolman | ❌ (только UID) |
| **Prusa Core / MK4S (Q1 2026+)** | — | ✅ **OpenPrintTag** на SLIX2 |
| Creality K2 / OpenSpool-принтеры | — | ✅ OpenSpool на NTAG215 |
| Без RFID | хранение в портале | ❌ |

**Запись формата нужна только для Prusa и OpenSpool-семейства.** На каждом — **один формат**, никаких "что выбрать". Конфликта NTAG215 (OpenSpool vs OpenTag3D) нет — OpenTag3D пока маргинален, можно отложить.

---

## Чип → формат — однозначное правило

- **SLIX2** → OpenPrintTag (Prusa)
- **NTAG215 / 216** → OpenSpool
- **MIFARE Classic 1K** → не пишем (Bambu — RSA-подпись, не сделать; Creality — пишется, но через MQTT/API проще)

UX-правило для пользователя:

- Приложил **SLIX2** → пишем Prusa
- Приложил **NTAG215** → пишем OpenSpool

Чип сам диктует формат → "профиль принтера на сушилке" не обязателен на старте.

---

## Что проверить дальше

1. **CBOR-кодер для OpenPrintTag** — где живёт? Логично делать на портале (Python/JS), сушилке слать готовые байты
2. Связь "катушка → принтер → Spoolman/Moonraker" — реализована ли в портале?

---

## Источники

- [OpenPrintTag — Prusa blog](https://blog.prusa3d.com/the-openprinttag-is-here-a-brand-new-nfc-tag-standard-for-smart-filament-is-now-shipped-with-a-new-redesigned-prusament-spool_123878/)
- [OpenPrintTag — Prusa KB](https://help.prusa3d.com/article/openprinttag_978161)
- [OpenPrintTag.org](https://openprinttag.org/)
- [Spoolman](https://github.com/Donkie/Spoolman)
- [Moonraker external integrations](https://moonraker.readthedocs.io/en/latest/external_api/integrations/)
