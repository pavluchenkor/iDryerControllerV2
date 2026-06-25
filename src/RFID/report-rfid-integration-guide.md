# Материалы по теме

https://github.com/Bambu-Research-Group/RFID-Tag-Guide

https://openspool.io/
https://www.tindie.com/products/spuder/openspool-mini/
https://github.com/spuder/openspool

поделка из бамбука
https://makerworld.com/ru/models/945595-ams-lite-enclosure-spool-holder-non-destructive-rf?from=search#profileId-912353
https://www.reddit.com/r/BambuLab/comments/1hbq5g6/does_anybody_know_which_connector_the_ams_lite/?utm_source=chatgpt.com


https://wiki.snapmaker.com/en/snapmaker_u1/troubleshooting/RFID_coil_replacement_guide

---

## Исследование: Snapmaker U1 / Polymaker RFID (апрель 2026)

**Железо:** MIFARE Classic 1K, ридер FM17550, ISO 14443 A/B.

**Ключи:** Snapmaker использует асимметричную криптографию — ключи нигде публично не опубликованы. Попытка перебора дефолтных ключей (FF×6, A0A1A2A3A4A5, D3F7×3, 00×6) провалилась на всех 4 секторах. Читать и писать родные теги Snapmaker без ключей — невозможно.

**Вывод для iDryer:**
- Детектируем UID (4 байта) → передаём на портал → пользователь привязывает вручную
- Перезаписать родной тег нельзя (нет ключей)
- Наклеить поверх NTAG215 — работает, PN532 читает ближний тег
- Snapmaker U1 с родной прошивкой не читает NTAG, с кастомной (paxx12) — читает OpenSpool/NTAG215

**Bambu:** ключи деривируются из UID (KDF реверс-инжинирован сообществом в 2024), читать можно, писать нельзя (RSA). Реализация KDF на RP2040 — возможна (~50 строк C++).

**Источники:**
- https://snapmakeru1-extended-firmware.pages.dev/rfid_support
- https://forum.snapmaker.com/t/u1-and-rfid-standard/39948
- https://github.com/paxx12/SnapmakerU1-Extended-Firmware/issues/333
- https://github.com/Bambu-Research-Group/RFID-Tag-Guide

---

## Лог реальных детектов на железе

```
[RFID] reader0 unit0 DETECTED uid=81:FB:6C:F0 type=MIFARE_1K   ← Snapmaker/Polymaker катушка
[RFID] reader0 unit0 DETECTED uid=04:C1:42:A1:74:26:81 type=NTAG215
```


# Инструкция: интеграция RFID в прошивку iDryer (RP2040)

> Для агента. Платформа: RP2040, arduino-pico core, PlatformIO.

---

## Структура файлов

```
src/
  rfid/
    IRfidDriver.h       ← абстрактный интерфейс (не трогать при смене чипа)
    RfidManager.h       ← менеджер: детект, read/write, isAvailable()
    RfidManager.cpp
    drivers/
      Pn532Driver.h     ← реализация под PN532
      Pn532Driver.cpp
      Pn5180Driver.h    ← реализация под PN5180 (добавить позже)
      Pn5180Driver.cpp
```

---

## platformio.ini

```ini
[env]
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = pico
framework = arduino
board_build.core = earlephilhower

lib_deps =
  https://github.com/Seeed-Studio/PN532.git

build_flags =
  -DNFC_INTERFACE_SPI
```

## Почему выбрали Seeed-Studio/PN532

- Нужна стабильность на `PlatformIO`, а не эмуляция.
- Проект сидит на `board_build.core = earlephilhower`, использует `SPI1` и кастомную разводку пинов на собственном контроллере.
- `Seeed-Studio/PN532` нормально упакован для PlatformIO (`src/`, `library.json`, SPI-флаг через `-DNFC_INTERFACE_SPI`).
- `elechouse/PN532` для этого проекта оказался неудобен именно как зависимость PlatformIO: raw git-подключение раскладывалось неполно и ломало include'ы.
- По API `Seeed` достаточно близок к `elechouse`, поэтому цена миграции низкая.
- Эмуляция не нужна, значит приоритет: стабильная сборка + SPI-чтение/запись UID/страниц.

---

## Пины (из схемы iDryer)

```
RP2040 GPIO12  →  PN532 MISO   (SPI1)
RP2040 GPIO14  →  PN532 SCK    (SPI1)
RP2040 GPIO15  →  PN532 MOSI   (SPI1)
RP2040 GPIO18  →  PN532 RST
RP2040 GPIO19  →  PN532 NSS/CS (SPI1)
```

PN532 модуль: установить джампер в режим SPI (DIP1=0, DIP2=1).

---

## 1. Абстрактный интерфейс — IRfidDriver.h

```cpp
#pragma once
#include <stdint.h>
#include <stddef.h>

// Результат операций
enum class RfidStatus {
    OK,
    NOT_FOUND,      // модуль не обнаружен при init
    NO_TAG,         // метка не поднесена
    READ_ERROR,
    WRITE_ERROR,
};

// Тип метки (детектируется автоматически при чтении UID)
enum class RfidTagType {
    UNKNOWN,
    MIFARE_CLASSIC_1K,
    NTAG213,
    NTAG215,
    NTAG216,
};

struct RfidTag {
    uint8_t     uid[7];
    uint8_t     uidLen;       // 4 (MIFARE) или 7 (NTAG)
    RfidTagType type;
};

// Интерфейс драйвера — реализуется под каждый чип отдельно
class IRfidDriver {
public:
    virtual ~IRfidDriver() = default;

    // Инициализация. Возвращает false если чип не найден.
    virtual bool init() = 0;

    // Опрос: есть ли метка в поле. Заполняет tag если есть.
    virtual RfidStatus poll(RfidTag &tag) = 0;

    // Чтение сырых данных (постранично для NTAG, поблочно для MIFARE)
    virtual RfidStatus readRaw(const RfidTag &tag,
                               uint8_t startPage,
                               uint8_t *buf,
                               size_t  len) = 0;

    // Запись сырых данных
    virtual RfidStatus writeRaw(const RfidTag &tag,
                                uint8_t startPage,
                                const uint8_t *data,
                                size_t         len) = 0;
};
```

---

## 2. Реализация под PN532 — Pn532Driver.h

```cpp
#pragma once
#include "IRfidDriver.h"
#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>

class Pn532Driver : public IRfidDriver {
public:
    // earlephilhower core: используем SPIClassRP2040 и remap SPI1-пинов.
    Pn532Driver(SPIClassRP2040 &spi,
                uint8_t         csPin,
                uint8_t         rstPin,
                uint8_t         misoPin,
                uint8_t         mosiPin,
                uint8_t         sckPin);

    bool       init()                                          override;
    RfidStatus poll(RfidTag &tag)                             override;
    RfidStatus readRaw(const RfidTag &tag, uint8_t startPage,
                       uint8_t *buf, size_t len)              override;
    RfidStatus writeRaw(const RfidTag &tag, uint8_t startPage,
                        const uint8_t *data, size_t len)      override;

private:
    SPIClass    &_spi;
    uint8_t      _csPin;
    uint8_t      _rstPin;
    uint8_t      _misoPin;
    uint8_t      _mosiPin;
    uint8_t      _sckPin;
    PN532_SPI   *_pn532spi = nullptr;
    PN532       *_nfc      = nullptr;

    RfidTagType detectType(uint8_t uidLen);
};
```

---

## 3. Реализация под PN532 — Pn532Driver.cpp

```cpp
#include "Pn532Driver.h"

Pn532Driver::Pn532Driver(SPIClass &spi, uint8_t csPin, uint8_t rstPin,
                         uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin)
    : _spi(spi), _csPin(csPin), _rstPin(rstPin),
      _misoPin(misoPin), _mosiPin(mosiPin), _sckPin(sckPin) {}

bool Pn532Driver::init() {
    // Хардварный сброс
    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(10);

    // earlephilhower core: SPI1 с явным remap пинов
    _spi.setRX(_misoPin);
    _spi.setTX(_mosiPin);
    _spi.setSCK(_sckPin);
    _spi.begin();

    _pn532spi = new PN532_SPI(_spi, _csPin);
    _nfc      = new PN532(*_pn532spi);

    _nfc->begin();
    delay(10);

    uint32_t ver = _nfc->getFirmwareVersion();
    if (!ver) return false;   // чип не найден

    _nfc->SAMConfig();
    return true;
}

RfidStatus Pn532Driver::poll(RfidTag &tag) {
    uint8_t uid[7] = {};
    uint8_t uidLen = 0;

    bool found = _nfc->readPassiveTargetID(
        PN532_MIFARE_ISO14443A, uid, &uidLen, 100  // 100мс таймаут
    );

    if (!found) return RfidStatus::NO_TAG;

    memcpy(tag.uid, uid, uidLen);
    tag.uidLen = uidLen;
    tag.type   = detectType(uidLen);
    return RfidStatus::OK;
}

RfidStatus Pn532Driver::readRaw(const RfidTag &tag, uint8_t startPage,
                                uint8_t *buf, size_t len) {
    // Для NTAG: читаем постранично (4 байта = 1 страница)
    size_t pages = (len + 3) / 4;
    for (size_t i = 0; i < pages; i++) {
        uint8_t tmp[4];
        if (!_nfc->mifareclassic_ReadDataBlock(startPage + i, tmp)) {
            // Для Seeed PN532 используем mifareultralight_* API
            if (!_nfc->mifareultralight_ReadPage(startPage + i, tmp)) {
                return RfidStatus::READ_ERROR;
            }
        }
        size_t copy = min((size_t)4, len - i * 4);
        memcpy(buf + i * 4, tmp, copy);
    }
    return RfidStatus::OK;
}

RfidStatus Pn532Driver::writeRaw(const RfidTag &tag, uint8_t startPage,
                                 const uint8_t *data, size_t len) {
    size_t pages = (len + 3) / 4;
    for (size_t i = 0; i < pages; i++) {
        uint8_t tmp[4] = {};
        size_t copy = min((size_t)4, len - i * 4);
        memcpy(tmp, data + i * 4, copy);
        if (!_nfc->mifareultralight_WritePage(startPage + i, tmp)) {
            return RfidStatus::WRITE_ERROR;
        }
    }
    return RfidStatus::OK;
}

RfidTagType Pn532Driver::detectType(uint8_t uidLen) {
    // MIFARE Classic всегда 4-байтный UID
    // NTAG всегда 7-байтный UID
    // Точный тип (213/215/216) определяется по размеру памяти тега
    // (можно уточнить чтением capability container, страница 3)
    if (uidLen == 4) return RfidTagType::MIFARE_CLASSIC_1K;
    if (uidLen == 7) return RfidTagType::NTAG215; // уточнять при необходимости
    return RfidTagType::UNKNOWN;
}
```

---

## 4. Менеджер — RfidManager.h

```cpp
#pragma once
#include "IRfidDriver.h"
#include <memory>

class RfidManager {
public:
    // Принимает любой драйвер через интерфейс
    explicit RfidManager(IRfidDriver *driver);

    // Вызвать один раз в setup()
    bool begin();

    // Проверять перед любым обращением к RFID
    bool isAvailable() const { return _available; }

    // Опрос: есть ли метка
    RfidStatus poll(RfidTag &tag);

    // Читать сырые байты с метки
    RfidStatus read(const RfidTag &tag, uint8_t startPage,
                    uint8_t *buf, size_t len);

    // Записать байты на метку
    RfidStatus write(const RfidTag &tag, uint8_t startPage,
                     const uint8_t *data, size_t len);

private:
    IRfidDriver *_driver;
    bool         _available = false;
};
```

---

## 5. Менеджер — RfidManager.cpp

```cpp
#include "RfidManager.h"

RfidManager::RfidManager(IRfidDriver *driver) : _driver(driver) {}

bool RfidManager::begin() {
    if (!_driver) return false;
    _available = _driver->init();
    if (!_available) {
        Serial.println("[RFID] module not found — disabled");
    } else {
        Serial.println("[RFID] ready");
    }
    return _available;
}

RfidStatus RfidManager::poll(RfidTag &tag) {
    if (!_available) return RfidStatus::NOT_FOUND;
    return _driver->poll(tag);
}

RfidStatus RfidManager::read(const RfidTag &tag, uint8_t startPage,
                              uint8_t *buf, size_t len) {
    if (!_available) return RfidStatus::NOT_FOUND;
    return _driver->readRaw(tag, startPage, buf, len);
}

RfidStatus RfidManager::write(const RfidTag &tag, uint8_t startPage,
                               const uint8_t *data, size_t len) {
    if (!_available) return RfidStatus::NOT_FOUND;
    return _driver->writeRaw(tag, startPage, data, len);
}
```

---

## 6. Инициализация в main.cpp

```cpp
#include "rfid/RfidManager.h"
#include "rfid/drivers/Pn532Driver.h"

// Пины из схемы iDryer
#define RFID_MISO  12
#define RFID_SCK   14
#define RFID_MOSI  15
#define RFID_RST   18
#define RFID_CS    19

// Создаём драйвер и менеджер глобально
Pn532Driver  rfidDriver(SPI1, RFID_CS, RFID_RST, RFID_MISO, RFID_MOSI, RFID_SCK);
RfidManager  rfid(&rfidDriver);

void setup() {
    Serial.begin(115200);
    rfid.begin();   // если модуль не найден — isAvailable() == false, всё остальное работает
}

void loop() {
    if (rfid.isAvailable()) {
        RfidTag tag;
        if (rfid.poll(tag) == RfidStatus::OK) {
            // новая метка — отправить UID на портал
            // formatUID(tag.uid, tag.uidLen) → "04:A3:2F:..."
        }
    }
    // остальная логика не зависит от RFID
}
```

---

## Как добавить PN5180 в будущем

1. Создать `src/rfid/drivers/Pn5180Driver.h` и `.cpp`, реализовать `IRfidDriver`
2. В `main.cpp` заменить:
   ```cpp
   // было:
   Pn532Driver rfidDriver(...);
   // стало:
   Pn5180Driver rfidDriver(...);
   ```
3. В `platformio.ini` добавить нужную либу и заменить флаг:
   ```ini
   build_flags = -DRFID_DRIVER_PN5180
   ```

`RfidManager`, весь портальный протокол, бизнес-логика — не меняются.

---

## Что НЕ делать

- Не обращаться к `PN532` / `PN5180` напрямую из бизнес-логики — только через `RfidManager`
- Не хардкодить пины внутри драйвера — передавать через конструктор
- Не вызывать методы `rfid.*` без проверки `rfid.isAvailable()`
- Не использовать I2C или UART для PN532 — только SPI (быстрее, нет ограничений на размер пакета)

---

## Поток данных по протоколу (апрель 2026)

> Исследование кода: `lib/idryer-core/src/uart/uart_protocol.h`,
> `lib/idryer-core/contracts/mqtt_contract.yaml`,
> `/Users/ruslanpavlucenko/Projects/iDryerPortal/backend/src/mqtt-telemetry/handlers/rfid.handler.ts`,
> `/Users/ruslanpavlucenko/Projects/iDryerPortal/backend/src/mqtt-telemetry/mqtt-api.types.ts`

### Архитектура цепочки

```
RP2040  ──UART──►  ESP32  ──MQTT──►  Portal Backend
                      ◄──MQTT──         ──WebSocket──►  Frontend
```

### Сообщения RP2040 → ESP32 (бинарный UART-протокол)

| Код    | Имя          | Описание |
|--------|--------------|----------|
| `0x14` | `Rfid`       | Событие tag_detected / tag_removed. Структура `RfidPayload`: event(1) + readerId(1) + tag[32](hex UID) + unitId(1) + pad(2) |
| `0x1A` | `RfidReadData` | Данные с метки, 6 фрагментов по 163 байта = 888 байт. Структура `RfidDataPayload`: readerId(1) + unitId(1) + tag[32] + fragment[163] + pad(2) |

### Сообщения ESP32 → RP2040 (команды)

| Код    | Имя          | Описание |
|--------|--------------|----------|
| `0x07` | `ReadRfid`   | Запрос прочитать тег. RP2040 отвечает 6 фрагментами `RfidReadData(0x1A)` |
| `0x08` | `WriteRfid`  | Не реализовано (TODO в command_handler.cpp) |
| `0x1B` | `RfidWriteData` | Фрагмент данных для записи на тег (888 байт, 6×163) |

### MQTT-топики (ESP32 → Portal)

Оба типа публикуются в один топик **`idryer/{token}/rfid`**, различаются наличием поля `format`:

```json
// Тип 1: RfidPayload — событие (нет поля format)
{
  "event": "tag_detected",   // или "tag_removed"
  "readerId": "0",
  "tag": "04:C1:42:A1:74:26:81",
  "unitId": "U1",
  "timestamp": "2026-04-10T12:00:00Z"
}

// Тип 2: RfidDataPayload — содержимое метки (есть поле format)
{
  "readerId": "0",
  "unitId": "U1",
  "tagId": "04:C1:42:A1:74:26:81",
  "format": "openprinttag",  // или "empty" / "unknown"
  "data": "<base64, 888 байт>",
  "timestamp": "2026-04-10T12:00:00Z"
}
```

### Логика Portal Backend (rfid.handler.ts)

1. `tag_detected` → ищет `Spool` по `rfidTag` в БД → автоматически устанавливает катушку на юнит
2. `tag_removed` → помечает `SpoolUsage.removedAt`, прерывает активную сессию сушки
3. `RfidDataPayload` (с `format`) → логирует, парсинг OpenPrintTag — TODO

### Текущий статус реализации

| Шаг | Статус |
|-----|--------|
| Детект тега, dump страниц в Serial | ✅ реализовано |
| `Rfid(0x14)` TagDetected → UART → ESP32 | 🔲 следующий шаг |
| `RfidReadData(0x1A)` 6 фрагментов → UART | 🔲 запланировано |
| `ReadRfid(0x07)` handler на RP2040 стороне | 🔲 запланировано |
| `WriteRfid / RfidWriteData` | 🔲 не реализовано (ESP32 тоже TODO) |

### Решение: auto-push vs on-demand

`ReadRfid(0x07)` — это команда **по запросу** от портала.
Portal backend **не посылает** её автоматически после `tag_detected`.
Вариант **auto-push** (RP2040 сам читает и шлёт `RfidReadData` после каждого TagDetected) — проще и достаточен для текущего MVP.
