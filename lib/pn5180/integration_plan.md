# Единый план интеграции iDryer ↔ принтеры

Архитектура связи: сушилка iDryer (PN5180) → портал → принтер (Klipper / Bambu / Prusa / OpenSpool).

---

## Связанные документы (источники правды по реализации)

- **`portal/docs/Open-tag/rfid-write-backend-plan.md`** — детальный план записи OpenPrintTag на стороне backend (триггеры, debounce, MQTT-контракт, Prisma)
- **`portal/docs/Open-tag/e-num.md`** — поля и enum'ы OpenPrintTag
- **`portal/docs/Open-tag/rfid-chunk-write-protocol.md`** — физика блочной записи ISO 15693 / SLIX2
- **`portal/docs/RFID/report-rfid-*.md`** — обзор архитектуры и выбора чипов
- **`docs/iDryerRP2040/BAMBU_EMULATION_PLAN.md`** — план Bambu emulation (частично реализован)
- **`idryer-core/contracts/mqtt_contract.yaml`** — UART/MQTT протокол (RFID write секции)
- **`lib/pn5180/rfid_standards_3dprint.md`** — обзор RFID-стандартов рынка
- **`lib/pn5180/rfid_write_strategy.md`** — стратегия выбора формата по чипу

Этот план — навигатор и общая картина. Конкретика реализации — в файлах выше.

---

## Как это будет работать (идея)

**1. Юзер вставил катушку в сушилку**
Сушилка читает RFID → шлёт UID на портал.

**2. Портал решает что это**
- Известная → подгружает данные
- Чужая с тегом любого формата, который умеем парсить (OpenPrintTag, OpenSpool, TigerTag, Bambu read-only, Creality, Anycubic, QIDI) → создаёт автоматом
- Пустая → юзер заполняет в портале (материал, цвет, вес)

**3. Портал говорит принтеру**
- **Klipper:** bridge у юзера в LAN дёргает Moonraker — "вот эта катушка активна"
- **Bambu:** backend публикует `commands/bambu_apply` → LINK сам шлёт MQTT принтеру в LAN. **Bridge не нужен**
- Spoolman есть → bridge синкает туда тоже

**4. Печать пошла**
Сушилка постоянно шлёт реальный вес → bridge льёт его в Moonraker. Расчётный вес Klipper не используем — только реальный.

Триггер "сейчас печатают": вес в доке падает ≥25г (порог из портального плана). Сушка/хранение этот порог не превышают.

**5. Триггеры записи тега** (см. портальный план записи)
- **A** — bind unknown tag → full write (80 блоков)
- **B** — стабильное падение веса ≥25г → patch write (aux-регион)
- **C** — кнопка "Force write" в портале → full write

Чип сам диктует формат (с возможностью override юзером):
- **SLIX2** → OpenPrintTag (Prusa)
- **NTAG215/216** → OpenSpool
- **MIFARE** → не пишем

**6. Запись физически**
Backend регенерирует бинарник детерминистически (CBOR + NDEF), считает diff блоков → шлёт `commands/rfid_write` с массивом `blocks[{addr, data}]` → LINK → UART → MCU пишет.

**7. Катушка унесена**
Тег = паспорт. Миграция возможна **двумя путями**:
- через тег (без портала) — для совместимых принтеров
- через портал (передача между юзерами с историей и настройками)

**Что юзер видит:** воткнул катушку → принтер сразу знает что это, сколько осталось, как печатать. Унёс — катушка автономна.

---

## Философия

**Катушка — автономный объект.**

- Катушка физически носит свои данные на RFID-метке (когда чип позволяет)
- Катушка может переезжать между сушилками, принтерами, **пользователями** (через портал)
- Портал хранит расширенные данные, историю, профили; метка — самодостаточный паспорт
- **Пишем тег всегда, когда чип технически позволяет** — ради переносимости

**Текущий контекст рынка:**
- Все non-Bambu принтеры у пользователей = Klipper
- Две интеграционных ветки: **Klipper** (через bridge у юзера) и **Bambu** (через LINK напрямую)

---

## Слои

1. **Identification** — сушилка читает RFID (UID + тип чипа), шлёт на портал
2. **Source of truth (Portal)** — БД катушек, привязки, **реальный вес**, профили сушки/печати, производитель, цвет, материал, история, передача между юзерами, кодеры тегов (CBOR/JSON)
3. **Routing** — портал знает целевой принтер для сушилки (per-сушилка mapping). В разделе устройств юзера — сушилки, принтеры, iHeater, грелки и т.д., все как сущности "device" с привязками между ними
4. **Bridge** — мост портал ↔ Klipper-stack у юзера в LAN (docker, web-морда, Moonraker API). **Для Bambu bridge не нужен — LINK пушит сам**
5. **Tag writing** — запись по триггерам A/B/C (см. портальный план)

---

## Компоненты

| Компонент | Ответственность |
|---|---|
| **iDryer firmware (RP2040, PN5180)** | чтение RFID, физическая запись блоков по команде LINK, телеметрия (реальный вес постоянно), профили сушки. **Не хранит секретов** (Bambu credentials, API keys) |
| **LINK (ESP32)** | MQTT-клиент к порталу, **MQTT-клиент к Bambu в LAN** (BambuClient + NVS credentials), сбор фрагментов RFID-данных, передача `WriteRfid` в RP2040 по UART |
| **Portal (backend)** | БД катушек/девайсов/профилей, **CBOR/JSON-кодеры**, RfidWriteService с per-spool мьютексом и debounce, генерация WritePacket, маршрутизация на bridge/LINK, передача катушек, архив |
| **Portal (frontend)** | управление катушками, привязки device↔device, диалог Bambu (готов), Force Write кнопка, индикатор `lastTagWriteAt`/`lastWrittenWeight`, статусы интеграций |
| **Bridge** (per-user, docker, LAN) | **только для Klipper**: Moonraker HTTP, опц. Spoolman-синк. Веб-морда: API keys, статус, лог, диагностика. Установка максимально простая |

---

## Правило "чип → формат" (с override)

UX: дефолт определяется типом метки, юзер может переопределить в портале.

| Чип | Дефолтный формат |
|---|---|
| SLIX2 (ISO 15693) | OpenPrintTag (Prusa) |
| NTAG215 / 216 (ISO 14443A) | OpenSpool |
| MIFARE Classic 1K | не пишем |

---

## Триггеры записи на тег

Из портального плана записи (`rfid-write-backend-plan.md`):

| Триггер | Режим | Условие |
|---|---|---|
| **A. Bind unknown** | full (80 блоков) | юзер привязал неопознанный UID к катушке |
| **B. Weight change** | patch (aux-регион) | стабильное изменение веса ≥25г + debounce 3с |
| **C. Force write** | full | кнопка в портале |

Дополнительно (наша надстройка):
- **Окончание программы сушки** → можно триггерить как C
- **Окончание печати** (если bridge получает событие от принтера) → C
- **Передача катушки между юзерами** → **только по кнопке** (не автотриггер)

**Никогда не пишем при каждом чтении.**

---

## Архитектура (схема)

```
┌──────────┐  RFID       ┌─────────┐  MQTT       ┌───────────┐
│ Filament │────────────▶│  iDryer │────────────▶│  Portal   │
│  (tag)   │◀────────────│ RP2040+ │◀────────────│ (backend) │
└──────────┘  blocks      │  LINK   │  cmds      │ + кодеры  │
                          │ (ESP32) │  (write,   │ + RfidWS  │
                          └────┬────┘   bambu)   │ + DB      │
                               │                 └─────┬─────┘
                               │ Bambu MQTT (LAN)      │
                               ▼                       │ HTTP/WS
                          ┌─────────┐                  ▼
                          │  Bambu  │       ┌───────────────────┐
                          │ printer │       │  Bridge per user  │
                          │  (LAN)  │       │  (docker, LAN)    │
                          └─────────┘       │  только Klipper   │
                                            │  + web UI:        │
                                            │   API keys        │
                                            │   статус/лог      │
                                            └────────┬──────────┘
                                                     │ HTTP
                                                     ▼
                                            ┌──────────────────┐
                                            │  Moonraker       │
                                            │ (+Spoolman опц.) │
                                            │  + Klipper       │
                                            └──────────────────┘
```

Ключевое отличие от первой версии: **Bambu идёт от LINK напрямую к принтеру в LAN**, не через bridge. Это упрощает деплой для Bambu-юзеров — им вообще не нужен docker.

---

## MVP по этапам (треки идут параллельно)

### Трек 1. Bambu (частично сделан)

| # | Этап | Состояние | Что юзер получит |
|---|---|---|---|
| 1.1 | Backend `POST /devices/:id/configure-bambu` | ✅ |  |
| 1.2 | Frontend диалог Bambu в DeviceShow + i18n | ✅ |  |
| 1.3 | LINK `BambuClient` + NVS credentials + обработчик `commands/bambu_config` | ❌ | Сушилка хранит конфиг Bambu, статус виден |
| 1.4 | LINK обработчик `commands/bambu_apply` + статус `bambu/status` | ❌ |  |
| 1.5 | Backend: на `tag_detected` публиковать `commands/bambu_apply` | ❌ | Воткнул катушку → AMS получил настройки филамента, печать с правильными температурами/цветом |

### Трек 2. Запись OpenPrintTag (Prusa)

| # | Этап | Состояние |
|---|---|---|
| 2.1 | TS-кодировщик OpenPrintTag (CBOR + NDEF, фиксированный uint16 для consumed_weight) + fixtures-тесты | ❌ |
| 2.2 | `RfidWriteService` (debounce, per-spool мьютекс, full/patch + diff блоков) | ❌ |
| 2.3 | Prisma: `Spool.lastWrittenWeight`, `Spool.lastTagWriteAt`, `FilamentSpec.emptyContainerWeightGrams` | ❌ |
| 2.4 | Триггеры A (bind), B (weight ≥25г debounce 3с), C (force endpoint) | ❌ |
| 2.5 | MQTT `commands/rfid_write` контракт + handler `rfid_write_status` | ❌ |
| 2.6 | Frontend: Force Write кнопка, индикатор `lastTagWriteAt` | ❌ |
| 2.7 | LINK: приём `commands/rfid_write`, проверка `expectedUid`, передача в RP2040 фрагментами | ❌ |
| 2.8 | RP2040: `WriteRfid` из заглушки → реальная запись, сборка фрагментов | firmware ✅ для физического слоя |

**Ценность:** юзер с любой Prusa-катушкой получает рабочий тег. На любом Prusa Core/MK4S (когда Prusa включит активный ридер — Q1 2026) катушка распознаётся. Включая чужие принтеры/у друга.

### Трек 3. Klipper bridge (новый, не покрыт в портале)

| # | Этап | Состояние |
|---|---|---|
| 3.1 | Bridge скелет (docker, веб-морда для API keys + статус) | ❌ |
| 3.2 | Транспорт portal ↔ bridge (MQTT + WebSocket) | ❌ |
| 3.3 | Bridge → Moonraker: установка active spool, real-weight push с частотой сушилки | ❌ |
| 3.4 | Опциональная Spoolman-интеграция (синк mapping + вес) | ❌ |
| 3.5 | Klipper macro / Python: `START_DRY material=PLA` → bridge → portal → сушилка (обратная интеграция) | ❌ |

**Ценность:** воткнул катушку — Mainsail/Fluidd показывают её активной, **реальный** вес во время печати (не расчётный). Из Klipper можно запустить сушку — кнопкой/макросом перед печатью.

### Трек 4. Запись OpenSpool (NTAG215/216)

| # | Этап | Состояние |
|---|---|---|
| 4.1 | TS-кодировщик OpenSpool JSON (по схеме `application/json` с полями protocol/version/type/color_hex/brand/min_temp/max_temp) | ❌ |
| 4.2 | Реюз `RfidWriteService` с другим encoder'ом (full write, без patch — JSON переменной длины) | ❌ |
| 4.3 | LINK: запись Type 2 NDEF на NTAG | ❌ |
| 4.4 | RP2040: уже готово (NTAG запись работает) | firmware ✅ |

**Ценность:** юзер с любой NTAG215-меткой получает катушку для OpenSpool-семейства. **Кто реально читает OpenSpool в stock:** Snapmaker U1 (альт. прошивка), сторонние ESP32+PN532 ридеры (юзер вешает на принтер сам). Creality K2 — НЕ совместим (свой AES-encrypted MIFARE формат).

### Трек 5. Автосоздание катушки из чужого тега

| # | Этап | Состояние |
|---|---|---|
| 5.1 | Парсеры на портале для всех известных форматов (OpenPrintTag, OpenSpool, TigerTag, Bambu read-only, Creality, Anycubic, QIDI) | ❌ |
| 5.2 | На событии `tag_detected` с UID без записи в БД → попытка распарсить → автосоздание с заполненными полями | ❌ |

**Ценность:** юзер купил Prusament — приложил к сушилке — катушка сразу в портале с материалом/цветом/брендом.

---

## Решённые архитектурные вопросы

| Вопрос | Решение |
|---|---|
| Где живёт CBOR/JSON-кодер | Portal backend (TS, `cbor-x`) |
| Где запускается bridge | Docker у юзера (LAN). Только для Klipper. Для Bambu bridge не нужен |
| Auth bridge ↔ Moonraker | API key, веб-морда bridge |
| Auth Bambu credentials | Хранятся **только в NVS LINK**, не на backend, не в RP2040 |
| Spoolman обязателен | Нет. Bridge работает напрямую с Moonraker, Spoolman синкается если есть |
| Real-weight sync частота | Постоянная, с частотой обновления сушилки |
| Источник веса при печати | Реальный из сушилки (приоритет). Расчёт Moonraker — только фолбэк |
| Mapping `idryer_spool ↔ external_id` | Bridge синкает автоматом по UID |
| Транспорт bridge ↔ portal | MQTT + WebSocket |
| Триггер записи тега | A/B/C из портального плана + кнопочные события (конец сушки/печати, передача катушки) |
| Чужие катушки | Автосоздание из любого парсимого формата |
| Удаление катушки | Не удаляется, архивируется |
| Веб-морда bridge | Статус, лог, диагностика, ввод API keys |
| Пишем тег всегда? | Да, когда чип позволяет — ради автономности |
| Конфликт NTAG215 формата | Дефолт — OpenSpool. OpenTag3D отложен |
| Routing катушка↔принтер | Per-сушилка mapping. В разделе устройств юзера — сушилки, принтеры, iHeater. Кнопка переключения для редких сценариев |
| Partial write веса | Детерминистическая регенерация всего бинарника + diff блоков. `consumed_weight` = CBOR uint16 фикс (3 байта). Patch = весь aux-регион атомарно |
| "Сейчас печатают?" | Не нужна Bambu-интеграция. Порог веса 25г разделяет сушку от печати (катушка в доке) |
| Bambu credentials | LINK NVS, не RP2040, не backend storage. Backend только проксирует через `commands/bambu_config` |
| Failure записи | Backend не ретраит. На следующий weight event порог сработает идемпотентно |

---

## Протокол портал ↔ сушилка для записи тега

**Уже спроектирован.** См. `idryer-core/contracts/mqtt_contract.yaml` (RFID write секции) и `portal/docs/Open-tag/rfid-write-backend-plan.md` §10 (конкретный MQTT-контракт `commands/rfid_write`).

Backend публикует:
```
idryer/{serial}/commands/rfid_write
{ jobId, unitId, readerId, expectedUid, mode: "full|patch", blocks: [{addr, data: base64}] }
```

LINK отвечает:
```
idryer/{serial}/rfid_write_status
{ jobId, state: "queued|writing|done|failed", blocksWritten, blocksTotal, error }
```

**Что доделать в LINK** (из BAMBU_EMULATION_PLAN.md и rfid-write-backend-plan.md):
- Приём `commands/rfid_write`, проверка `expectedUid`
- Передача в RP2040 через `WriteRfid` UART + `RfidWriteData` фрагментами
- Публикация `rfid_write_status`
- BambuClient (NVS credentials, MQTT к принтеру в LAN)
- Обработчик `commands/bambu_config` и `commands/bambu_apply`

UX-нюанс: запись OpenPrintTag ~1.3с — катушка должна оставаться на ридере (тривиально, отметить и забыть).

---

## Состояние катушки

Катушка **либо в сушилке (в доке), либо неизвестно где**. Real-weight sync работает только когда катушка в сушилке. Усложнять не надо.

Killer-инсайт из портального плана: если ридер видит тег — катушка в доке. Это снимает целый класс проблем "где сейчас катушка".

---

## Отложено (future work)

- **Multi-color / MMU (Bambu AMS, Prusa MMU, Klipper-MMU)** — несколько активных катушек в слотах. Хотим, если реализуемо. Не в MVP
- **Offline-режим bridge** — локальный кэш данных портала. Пока фантастика
- **Per-катушка routing** — выбор целевого принтера для конкретной катушки. Пока per-сушилка mapping покрывает кейсы
- **Версионирование форматов** — watcher на бэке за репами спек. Пока ручной процесс
- **Bambu Cloud режим** — сейчас только Bambu LAN. Cloud требует bambu-аккаунта-токена

---

## Что не делаем (явно)

- **MIFARE Classic запись для Bambu** — невозможно (RSA-подпись)
- **OpenTag3D** — пока маргинален, отмечен в `rfid_standards_3dprint.md` для истории, в реализации не идёт
- **Универсальный формат "одна метка для всех"** — не существует
- **Запись при каждом чтении тега** — только по триггерам A/B/C
- **Хранение Bambu credentials в RP2040 / в backend storage** — только в NVS LINK
- **Backend ретраи записи** — на следующий weight event порог сработает идемпотентно
