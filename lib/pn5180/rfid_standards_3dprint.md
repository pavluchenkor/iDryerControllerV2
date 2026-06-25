# RFID/NFC стандарты меток для катушек филамента в 3D-печати

Источники: GitHub-репозитории спецификаций (прочитаны напрямую), официальные сайты.
Не агрегаторы.

---

## 1. Открытые стандарты

---

### OpenPrintTag — Prusa Research

- **Сайт**: [openprinttag.org](https://openprinttag.org) · [specs.openprinttag.org](https://specs.openprinttag.org)
- **Спецификация (GitHub)**: [OpenPrintTag/openprinttag-specification](https://github.com/OpenPrintTag/openprinttag-specification) · [prusa3d/OpenPrintTag](https://github.com/prusa3d/OpenPrintTag) (зеркало Prusa)
- **RF-стандарт**: **ISO/IEC 15693-3 (NFC-V)** — не 14443A
- **Поддерживаемые чипы**:

  | Чип | Память (всего) | Пользовательская | Статус |
  |---|---|---|---|
  | NXP ICODE SLIX | 112 байт (28 блоков × 4) | ~112 байт | Совместим, но мало места для расширенных полей |
  | **NXP ICODE SLIX2** | 320 байт (80 блоков × 4) | **316 байт** (блок 0 = CC) | **Рекомендован**, спека написана под него |

  Новые Prusament с маркировкой «NFC» поставляются с ICODE SLIX2 (с 2025 года).
- **Desktop-запись**: требуется ридер с поддержкой ISO 15693 — **ACR1552U** (~$60). ACR122U ISO 15693 **не поддерживает**.
- **Кодирование данных**: NDEF (MIME type `application/vnd.openprinttag`) + **CBOR** (Concise Binary Object Representation, RFC 7049)
- **Структура payload**: три CBOR-секции — `meta` (смещения регионов), `main` (статика: материал, бренд, GTIN и т.д.), `aux` (динамика: остаток материала, workgroup — пишет принтер)
- **Шифрование**: нет. Защита записи через `PROTECT PAGE` команду SLIX2 (опционально, только главный регион)
- **Ключевые поля**: `brand_uuid`, `material_type`, `material_name`, `color_rgba`, `gtin`, `instance_uuid`
- **Версионирование**: без явного номера версии — новые ключи добавляются без поломки совместимости; при breaking change меняется MIME type
- **Подтверждено из**: `docs_src/nfc_technical_details.md` и `docs_src/nfc_data_format.md` репозитория спецификации

> **Важно для iDryer**: PN5180 поддерживает ISO 15693 — OpenPrintTag читается.

---

### OpenTag3D — OpenTag3D Consortium

- **Сайт**: [opentag3d.info](https://opentag3d.info) · [Спецификация](https://opentag3d.info/spec.html)
- **GitHub**: [queengooborg/OpenTag3D](https://github.com/queengooborg/OpenTag3D)
- **RF-стандарт**: **ISO/IEC 14443 Type A**, NDEF Type 2 Tag
- **Чипы** (от минимального к максимальному):

  | Чип | Ёмкость | Режим |
  |---|---|---|
  | NTAG213 | 144 байт | Core (минимум) |
  | SLIX2 | 320 байт | Core + Extended |
  | NTAG215 | 504 байт | Core + Extended |
  | NTAG216 | 888 байт | Core + Extended |

- **MIME type**: `application/opentag3d`
- **Кодирование**: бинарная memory map (не JSON). Строки UTF-8, числа big-endian unsigned
- **Температуры**: хранятся в °C, делённых на 5
- **Шифрование**: нет, принципиально (open standard)
- **Механическое расположение**: центр метки — 56.0 мм от центра катушки; не глубже 4 мм от поверхности; по одной метке на каждой торцевой стороне
- **Участники консорциума**: Gooborg Studios, Polar Filament + ряд производителей филамента (American Filament, Numakers, 3D Fuel, Ecogenesis Biopolymers)
- **Подтверждено из**: `spec.md` репозитория

---

### OpenSpool — сообщество (spuder)

- **GitHub**: [spuder/OpenSpool](https://github.com/spuder/OpenSpool)
- **RF-стандарт**: ISO/IEC 14443 Type A
- **Чип**: **NTAG215 / NTAG216** (минимум 500+ байт из-за JSON)
- **Кодирование**: NDEF, MIME type `application/json`, plain JSON:

  ```json
  {
    "protocol": "openspool",
    "version": "1.0",
    "type": "PLA",
    "color_hex": "FFAABB",
    "brand": "Generic",
    "min_temp": "220",
    "max_temp": "240"
  }
  ```

- **Шифрование**: нет
- **Аппаратная часть**: ESP32 + PN532, открытая схема (Gerber в репо)
- **Интеграции**: Bambu (MQTT), OctoPrint (в разработке), Klipper (планируется)
- **Подтверждено из**: `README.md` репозитория

---

### TigerTag — TigerTag Project

- **Сайт**: [tigertag.io](https://tigertag.io)
- **GitHub спецификация**: [TigerTag-Project/TigerTag-RFID-Guide](https://github.com/TigerTag-Project/TigerTag-RFID-Guide)
- **RF-стандарт**: ISO/IEC 14443 Type A
- **Чип**: **NTAG213** (144 байт, страницы 4–39)
- **Кодирование**: бинарный формат, big-endian, фиксированная раскладка полей:

  | Поле | Байт | Описание |
  |---|---|---|
  | ID TigerTag | 4 | Идентификатор формата (напр. `0x5BF59264` = offline) |
  | ID Product | 4 | `0xFFFFFFFF` = Maker/offline |
  | ID Material | 2 | Тип материала (PLA=`0x954B`, ABS=`0x5042` и др.) |
  | ID Diameter | 1 | `0x38`=1.75mm, `0xDD`=2.85mm |
  | ID Aspect 1+2 | 2 | Внешний вид (silk, matte и др.) |
  | ID Type | 1 | `0x8E`=филамент, `0xAD`=смола |
  | ID Brand | 2 | ID бренда (открытая БД на GitHub) |
  | ID Unit | 1 | Единица измерения |
  | Color1 RGBA | 4 | |
  | Color2+3 RGB | 6 | |
  | Measure | 3 | Вес/объём |
  | Nozzle Temp Min/Max | 2 | °C |
  | Dry Temp / Time | 2 | |
  | Bed Temp Min/Max | 2 | |
  | Time Stamp | 4 | секунды с 01.01.2000 GMT |
  | Reserved | 12 | |
  | Emoji | 4 | UTF-8 emoji |
  | Custom Message | 28 | свободный текст |
  | Signature (ECDSA-P256) | 64 | опционально, для фабричных меток |

- **Шифрование**: нет. Опциональная цифровая подпись ECDSA-P256 для производителей.
- **Подтверждено из**: `README.md` репозитория (полная таблица полей)

---

## 2. Проприетарные стандарты

---

### Bambu Lab AMS (X1, P1, A1 серии)

- **RF-стандарт**: ISO/IEC 14443 Type A
- **Чип**: **MIFARE Classic 1K**
- **GitHub RE**: [Bambu-Research-Group/RFID-Tag-Guide](https://github.com/Bambu-Research-Group/RFID-Tag-Guide)
- **Ключи**: деривируются из UID по известной KDF (с ноября 2024 публично). `deriveKeys.py [UID]` → 16 ключей. Permission Bits всегда `87 87 87 69`
- **Backdoor**: в 2024 найден аппаратный backdoor FM11RF08S ([eprint.iacr.org/2024/1275](https://eprint.iacr.org/2024/1275.pdf)). Скрипт `fm11rf08s_recovery` в Proxmark3 Iceman ≥ v4.18994 извлекает все ключи за ~15–20 мин без AMS
- **Подпись**: **RSA-2048**, занимает сектора 10–15 (целиком). Изменение любого байта → тег отклоняется принтером
- **Клонирование**: ✅ возможно — Magic Gen2 или FUID тег с идентичным UID + данными + RSA-подписью. Gen1 не работает (AMS проверяет команду `0x40`)
- **Запись своих данных**: ❌ — без приватного RSA-ключа Bambu создать валидную подпись невозможно
- **Порядок байт**: Little Endian

Структура данных (reverse-engineered):

| Блок | Содержимое |
|---|---|
| 1 | Tray Info Index (Material Variant ID + Material ID, начинаются с "GF") |
| 2 | Тип филамента (строка, 16 байт, напр. "PLA Basic") |
| 4 | Детальный тип ("PLA Matte", "PLA-CF" и т.д.) |
| 5 | Цвет RGBA (4B) + вес катушки uint16 LE (г) + диаметр float LE (мм) |
| 6 | Темп сушки, время сушки, тип стола, темп стола, макс/мин темп хотенда |
| 8 | X Cam info (12B) + диаметр сопла float LE |
| 9 | Tray UID (16B строка) |
| 10 | Ширина катушки uint16 LE (мм×100) |
| 12 | Дата/время производства ASCII (`YYYY_MM_DD_HH_MM`) |
| 14 | Длина филамента uint16 LE (метры?) |
| 16 | Доп. цвет: Format ID (2B) + Color Count (2B) + Color2 ABGR (4B) |
| 10–15 | RSA-2048 подпись |

---

### Creality CFS (K2 Plus и совместимые)

- **RF-стандарт**: ISO/IEC 14443-A
- **Чип**: **MIFARE Classic 1K**
- **GitHub RE**: [Bambu-Research-Group/RFID-Tag-Guide/CrealityRfid.md](https://github.com/Bambu-Research-Group/RFID-Tag-Guide/blob/main/CrealityRfid.md)
- **Шифрование**: двухслойное:
  1. **MIFARE Key A** — генерируется из UID тега: `повтор UID до 16 байт → AES-128(u_key, CBC, IV=0) → первые 6 байт = Key A`
  2. **Данные** в блоках 4–6 — зашифрованы **AES-128 CBC** (IV = нули) ключом `d_key`
  - Оба ключа **захардкожены** в прошивке Creality и известны сообществу ([cfs-programmer/TAG_FORMAT.md](https://github.com/srobinson9305/cfs-programmer/blob/main/docs/TAG_FORMAT.md))
- **iOS**: не поддерживается (Apple блокирует MIFARE Classic на уровне ОС)
- **Запись**: возможна. Сообщество пишет кастомные теги для стороннего филамента на обычные MIFARE Classic 1K. Требуются два тега на катушку (передний и задний — идентичные данные).
- **Формат данных** в блоках 4–6 (после расшифровки, ASCII, 48 байт):

  ```
  AAA BBBBB CCCC DDDDD #EEEEEE FFFFFF GGGGGGGGGGGGGGGGGG
  ```

  | Байты | Длина | Описание |
  |---|---|---|
  | 0–4 | 5 | Date code (ASCII, кодировка не до конца RE) |
  | 5–8 | 4 | Vendor ID (`0276` = Creality, `0000` = Generic) |
  | 9–10 | 2 | Batch / Unknown |
  | 11–16 | 6 | Filament Type ID (`101001`=PLA, `101002`=PETG, `101003`=ABS, `101004`=TPU, `101005`=Nylon) |
  | 17–23 | 7 | Color: `0RRGGBB` (ведущий ноль + 6-digit hex) |
  | 24–27 | 4 | Длина филамента в метрах, hex (`0330` = 816м = ~1кг PLA) |
  | 28–33 | 6 | Serial number |
  | 34–47 | 14 | Reserved (нули) |

- **GitHub инструменты сообщества**: [DnG-Crafts/K2-RFID](https://github.com/DnG-Crafts/K2-RFID) · [srobinson9305/cfs-programmer](https://github.com/srobinson9305/cfs-programmer) · [soylentOrange/K2-RFID](https://github.com/soylentOrange/K2-RFID)
- **Источник формата**: [TAG_FORMAT.md](https://github.com/srobinson9305/cfs-programmer/blob/main/docs/TAG_FORMAT.md)

---

### QIDI Box

- **RF-стандарт**: ISO/IEC 14443-A, 13.56 МГц, 106 Kbit/s
- **Чип**: **FM11RF08S** (Fudan Microelectronics) — MIFARE Classic совместимый клон
- **Память**: 16 секторов × 4 блока × 16 байт
- **Данные хранятся**: Sector 1, Block 0 — только **3 байта**:
  - `bit[0]` — Material Code (1–50, напр. PLA, ABS, PETG, TPU и др.)
  - `bit[1]` — Color Code (1–24)
  - `bit[2]` — Manufacturer Code (по умолчанию: 1)
- **Кодирование**: HEX
- **Шифрование**: M1-совместимое (стандартная MIFARE Classic защита)
- **GitHub RE**: [LexyGuru/Qidi_RFID_App](https://github.com/LexyGuru/Qidi_RFID_App) · [n0cloud/qidi-box-rfid-manager](https://github.com/n0cloud/qidi-box-rfid-manager)
- **Официальная документация**: [QIDI Wiki — QIDIBOX RFID](https://wiki.qidi3d.com/en/QIDIBOX/RFID)

---

### Anycubic ACE

- **RF-стандарт**: ISO/IEC 14443-A
- **Чип**: **NTAG213** или **NTAG215** (MIFARE Ultralight C совместимые) — оба протестированы и работают
- **Минимум**: 36 страниц, 144 байта user r/w области (т.е. NTAG213 достаточно)
- **Кодирование**: бинарное, little-endian, page-based (страницы по 4 байта):

  | Страница | Данные | Описание |
  |---|---|---|
  | 4 | `7B 00 65 00` | magic byte / data len |
  | 5–8 | ASCII | SKU (напр. `AHPLLB-103`) |
  | 10–13 | ASCII | Brand (напр. `AC`) |
  | 15–18 | ASCII | Type (напр. `PLA`) |
  | 20 | ABGR | Цвет (напр. `FF 00 FF 00`) |
  | 24 | uint16 LE × 2 | Extruder temp min/max (°C) |
  | 29 | uint16 LE × 2 | Hotbed temp min/max (°C) |
  | 30 | uint16 LE × 2 | Диаметр (мм×?) / длина (м) |
  | 31 | uint32 LE | Неизвестно |

- **Шифрование**: нет (NTAG, открытая запись)
- **GitHub RE**: [DnG-Crafts/ACE-RFID](https://github.com/DnG-Crafts/ACE-RFID) · [Molodos/anycubic-nfc-filament](https://github.com/Molodos/anycubic-nfc-filament)
- **Примечание**: SimplyPrint ошибочно указывал MIFARE Classic — это не так, чип ISO 14443-A NTAG-семейства

---

### Snapmaker U1 (штатная прошивка)

- **Чип**: **MIFARE Classic 1K** (штатная прошивка)
- **Альтернативная прошивка**: поддерживает NTAG215 + OpenSpool
- **Источник**: [snapmakeru1-extended-firmware.pages.dev/rfid_support](https://snapmakeru1-extended-firmware.pages.dev/rfid_support)

---

## 3. Базовый физический уровень — сводка

| Стандарт | Частота | Типичные чипы | Применение в 3D-печати |
|---|---|---|---|
| ISO/IEC 14443 Type A | 13.56 МГц | MIFARE Classic 1K, NTAG213/215/216 | Bambu, Creality, OpenTag3D, OpenSpool, TigerTag |
| ISO/IEC 15693 (NFC-V) | 13.56 МГц | ICODE SLIX2 | **OpenPrintTag** (Prusa) |

**PN5180 поддерживает оба RF-стандарта**: ISO 14443A и ISO 15693 — покрывает все актуальные открытые стандарты и большинство проприетарных.

---

## 4. Совместимость ридеров

Источники: `report-rfid-chip-selection.md` / `report-rfid-overview.md` (iDryerPortal), верифицировано по спецификациям чипов.

| Ридер | MIFARE Classic 1K | NTAG213/215/216 | ICODE SLIX/SLIX2 (ISO 15693) | iOS | Android | Цена |
|---|---|---|---|---|---|---|
| **PN5180** (SPI) | ✅ | ✅ | ✅ | — | — | ~$5–8 |
| **PN532** (SPI/I2C) | ✅ | ✅ | ❌ | — | — | ~$3 |
| **RC522** | ✅ | ✅ | ❌ | — | — | ~$1 |
| **ACR1552U** (USB) | ✅ | ✅ | ✅ | — | — | ~$60 |
| **ACR122U** (USB) | ✅ | ✅ | ❌ | — | — | ~$30 |
| **Смартфон Android** | ❌ | ✅ | ✅ | — | — | — |
| **Смартфон iOS** | ❌ | ✅ | ✅ | — | — | — |

> ❌ MIFARE Classic на iOS/Android: Apple и Google блокируют MIFARE Classic на уровне ОС — это аппаратное ограничение, не программное.

---

## 5. Статус поддержки в OpenSpool (как индикатор зрелости RE)

Из `README.md` репозитория OpenSpool (актуально на момент составления):

| Протокол | Чтение | Запись | Чип |
|---|---|---|---|
| OpenSpool | ✅ | ✅ | NTAG215/216 |
| TigerTag | 🚧 в разработке | 🚧 | NTAG213 |
| Bambu | 🚧 в разработке | ❌ | MIFARE Classic 1K |
| OpenTag3D | 🚧 в разработке | 🚧 | NTAG213/215/216 |
| Creality | 🗓️ запланировано | 🗓️ | MIFARE Classic 1K |
| OpenPrintTag (Prusa) | ❓ | ❓ | ICODE SLIX/SLIX2 (ISO 15693) |
| Elegoo | 🔍 исследуется | 🔍 | ❓ |
| Anycubic | 🔍 исследуется | 🔍 | NTAG213/215 |
