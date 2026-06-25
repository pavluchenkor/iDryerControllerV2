# OpenSpool Workflow -> рекомендации для iDryer

Документ фиксирует только две вещи:

- проверяемые факты по коду `OpenSpool`;
- рекомендации, как перенести ту же логику в `iDryer`.

Репозиторий-источник: <https://github.com/spuder/OpenSpool>

## 1. Что именно делает OpenSpool

`OpenSpool` реализует цепочку:

`NFC tag with OpenSpool JSON -> parse/validate -> build Bambu MQTT payload -> publish ams_filament_setting`

Проверка:
- `pn532_rfid-solo.yaml` читает `application/json`, проверяет `protocol == "openspool"` и публикует результат `bambulabs::generate_mqtt_payload(...)`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/pn532_rfid-solo.yaml#L41-L115>
- `bambu.h` строит payload `ams_filament_setting`: <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L96-L163>

Рекомендация для `iDryer`:
- повторять именно этот workflow;
- не делать отдельную "эмуляцию Bambu RFID";
- Bambu-логика должна строиться вокруг `read tag -> build MQTT payload -> publish to printer`.

## 2. Printer Settings в OpenSpool

OpenSpool поднимает такие printer-side поля:

- `Printer Model`
- `Printer Serial Number`
- `Printer Lan Access Code`
- `Printer IP Address`
- `MQTT Connection`
- `Restart OpenSpool`

Проверка:
- `bambu_printer.yaml`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/bambu_printer.yaml#L1-L127>
- `mqtt_bambu_lan.yaml`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/mqtt_bambu_lan.yaml#L1-L114>

Факты по runtime:

- `Printer IP Address` задаёт broker address через `id(bambu_mqtt).set_broker_address(...)`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/bambu_printer.yaml#L87-L119>
- `Printer Lan Access Code` задаёт MQTT password через `id(bambu_mqtt).set_password(...)`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/bambu_printer.yaml#L54-L86>
- `Printer Serial Number` участвует в connect gating и нужен для topic `device/<serial>/request`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/bambu_printer.yaml#L19-L53>, <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L94-L121>, <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/pn532_rfid-solo.yaml#L109-L114>
- `MQTT Connection` это только индикатор `id(bambu_mqtt)->is_connected()`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/mqtt_bambu_lan.yaml#L99-L114>
- `Printer Model` сейчас сохранён в UI, но в коде помечен как `currently unused`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/bambu_printer.yaml#L2-L17>

Рекомендация для `iDryer`:

- хранить на `LINK` один активный Bambu config:
  - `bambu_model`
  - `bambu_ip_address`
  - `bambu_serial_number`
  - `bambu_lan_access_code`
- повторить connect gating: подключаться только если заполнены `IP + Serial + Lan Access Code`;
- `Printer Model` хранить в конфиге `LINK`, но пока не использовать в runtime;
- не делать multi-printer profiles в MVP.

## 3. MQTT transport в OpenSpool

Факты:

- broker задаётся динамически, порт фиксирован `8883`;
- username фиксирован `bblp`;
- пароль берётся из `Lan Access Code`;
- включён TLS CA;
- `enable_on_boot: false`, соединение включается после проверки credentials.

Проверка:
- <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/mqtt_bambu_lan.yaml#L4-L60>
- <https://github.com/spuder/OpenSpool/blob/main/firmware/common.yaml#L18-L67>

Рекомендация для `iDryer`:

- в `LINK` повторить те же transport-параметры:
  - port `8883`
  - username `bblp`
  - password = `Lan Access Code`
  - topic `device/<serial>/request`
- credentials хранить только на `LINK`, не передавать их в `RP2040`.

## 4. Filament Settings в OpenSpool

OpenSpool реально поднимает в UI:

- `Filament Type`
- `Filament Variant`
- `Filament Brand`
- `Filament Color`
- `Include alpha value in filament color`
- `Filament Min Temp`
- `Filament Max Temp`
- `Filament alpha value`
- `Filament Brand Code`
- `Filament Color Hex`
- `Filament Color Hex Bambu`
- `Filament Sub Brand`
- `Upload Settings`

Проверка:
- `filament.yaml`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L5-L340>

Рекомендация для `iDryer`:

- не переносить весь этот UI в `RP2040`;
- если нужные material data уже есть в модели спула, использовать их;
- в `LINK/frontend/backend` обеспечить эквивалентные runtime-данные:
  - material type
  - brand
  - color
  - min temp
  - max temp

## 5. Цвет: `Filament Color Hex` и `Filament Color Hex Bambu`

Факты:

- `Filament Color` в OpenSpool это palette select, а не свободный ввод hex;
- palette маппится в `Filament Color Hex`;
- отдельный `Filament Color Hex Bambu` это RGBA-значение, которое собирается из RGB + alpha;
- при ручном publish используется либо `filament_color_hexaa`, либо `filament_color_hex + "FF"`;
- при auto-flow `bambu.h` делает ту же нормализацию: `RRGGBB -> RRGGBBFF`, `RRGGBBAA -> as is`.

Проверка:
- palette mapping: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L90-L186>
- alpha fields: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L187-L281>
- text sensors `filament_color_hex` / `filament_color_hexaa`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L282-L333>
- manual publish color handling: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L94-L121>
- auto publish color handling: <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L126-L149>

Рекомендация для `iDryer`:

- если у спула уже есть корректный `colorHex`, отдельный Bambu color field не нужен;
- в Bambu payload всегда отправлять RGBA;
- нормализация должна быть одна:
  - `RRGGBB -> RRGGBBFF`
  - `RRGGBBAA -> RRGGBBAA`

## 6. `Filament Type`, `Brand`, `Variant`, `Brand Code`, `Sub Brand`

Факты:

- `Filament Type` задаёт список поддерживаемых типов: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L5-L47>
- `Filament Variant` задаёт варианты `Basic / Matte / Metal / Impact / G / W`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L47-L68>
- `Filament Brand` задаёт бренды `Generic / Bambu / Overture / PolyTerra / PolyLite / Sunlu`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L69-L89>
- `generate_filament_brand_code` вычисляет `Filament Brand Code` и `Filament Sub Brand`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L122-L261>
- `bambu.h` содержит таблицы `filament_mappings`, `brand_specific_codes` и `get_bambu_code(...)`: <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L10-L93>

Практически важные коды:

- `PLA + Bambu + Basic -> GFA00`
- `PLA + Bambu + Matte -> GFA01`
- `PLA + Bambu + Metal -> GFA02`
- `PLA + Bambu + Impact -> GFA03`
- `PLA + Generic -> GFL99`
- `PLA + PolyTerra -> GFL01`
- `PLA + PolyLite -> GFL00`
- `PLA + Sunlu -> GFSNL03`
- `PETG + Bambu -> GFG00`
- `PETG + Generic -> GFG99`
- `PETG + Sunlu -> GFSNL08`
- `ABS + Bambu -> GFB00`
- `ABS + Generic -> GFB99`
- `ASA + Bambu -> GFB01`
- `ASA + Generic -> GFB98`
- `TPU + Bambu -> GFU01`
- `TPU + Generic -> GFU99`
- `PA-CF + Bambu -> GFN03`
- `PA-CF + Generic -> GFN98`
- `Support + G -> GFS01`
- `Support + W -> GFS00`

Проверка:
- `automation.yaml`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L122-L261>
- `bambu.h`: <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L10-L93>

Рекомендация для `iDryer`:

- сделать один источник истины для Bambu material code mapping;
- не размазывать эту таблицу по `frontend`, `backend` и `LINK`;
- переносить mapping по логике `OpenSpool`, а не упрощать до `PLA/PETG/ABS`.

## 7. Температуры

Факты:

- `Filament Min Temp` и `Filament Max Temp` ограничены диапазоном `150..300`, шаг `5`;
- UI синхронизирует min/max, чтобы они не пересекались;
- `generate_filament_temperatures` подставляет дефолты по типу/бренду;
- в Bambu payload уходят именно `nozzle_temp_min` и `nozzle_temp_max`.

Проверка:
- temperature fields: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L211-L281>
- default temperatures: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L262-L340>
- payload fields: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L111-L116>, <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L145-L149>

Рекомендация для `iDryer`:

- использовать существующие температуры спула/материала;
- на выходе builder всегда должен формировать `nozzle_temp_min` и `nozzle_temp_max`.

## 8. `Upload Settings` в OpenSpool

Факты:

- `Upload Settings` это отдельная кнопка в UI;
- она запускает script `publish_filament_setting`;
- это немедленный publish в Bambu, а не запись NFC;
- publish идёт в topic `device/<serial>/request`;
- payload содержит:
  - `command = "ams_filament_setting"`
  - `ams_id = 255`
  - `tray_id = 254`
  - `tray_color`
  - `nozzle_temp_min`
  - `nozzle_temp_max`
  - `tray_type`
  - `setting_id = ""`
  - `tray_info_idx = filament_brand_code`

Проверка:
- кнопка: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/filament.yaml#L334-L340>
- script `publish_filament_setting`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L31-L121>

Рекомендация для `iDryer`:

- в существующем `LINK/Bambu` разделе нужен ручной action уровня `Test apply`;
- ручной `Test apply` и авто-apply по RFID должны сходиться в один publish path на `LINK`.

## 9. Формат тега, который пишет OpenSpool

Факты:

- `Write NFC` пишет NDEF MIME `application/json`;
- JSON тега содержит только:
  - `version`
  - `protocol`
  - `color_hex`
  - `type`
  - `min_temp`
  - `max_temp`
  - `brand`
- в тег не пишутся:
  - `variant`
  - `brand_code`
  - `sub_brand`
  - `printer model`
  - `printer serial`
  - `ams_id`
  - `tray_id`

Проверка:
- `Write NFC`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/pn532_rfid-solo.yaml#L173-L318>
- preview JSON: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L3-L30>

Рекомендация для `iDryer`:

- если цель совместимость с `OpenSpool`, tag payload должен совпадать с этим составом;
- для Bambu workflow не нужно добавлять Bambu credentials или printer metadata в тег.

## 10. Ограничение `variant` в OpenSpool

Факты:

- `variant` влияет на `Filament Brand Code` и `Filament Sub Brand`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L136-L167>, <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L246-L257>
- `variant` участвует в ручном `Upload Settings` через `filament_brand_code`: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L94-L121>
- `variant` не сохраняется в tag JSON: <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/automation.yaml#L13-L24>, <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/pn532_rfid-solo.yaml#L267-L280>
- `generate_mqtt_payload()` в auto-flow использует `get_bambu_code(type, brand)`, без `variant`: <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L56-L93>, <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L147-L149>

Рекомендация для `iDryer`:

- при переносе логики учесть, что auto-flow по NFC в модели `OpenSpool` не восстанавливает `variant`;
- если требуется поведение строго как в `OpenSpool`, auto-flow нужно строить только из полей тега `type + brand + color + temperatures`.

## 11. Auto RFID publish: solo mode

Факты:

- на `on_tag` OpenSpool читает NDEF, ищет `application/json`, парсит JSON и проверяет `protocol == "openspool"`;
- при валидном payload и активном MQTT публикует в topic `device/<serial>/request`;
- в solo mode используется:
  - `ams_id = 255`
  - `tray_id = 254`

Проверка:
- <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/pn532_rfid-solo.yaml#L41-L115>

Рекомендация для `iDryer`:

- на стороне `RP2040` достаточно детектить тег и передавать его содержимое в `LINK`;
- publish в Bambu должен делать `LINK`, не `RP2040`.

## 12. Auto RFID publish: AMS mode

Факты:

- в `pn532_rfid-ams.yaml` mapping reader -> `(ams_id, tray_id)` задан явно в конфиге;
- `reader_spi_1` публикует с `(0, 0)`;
- `reader_spi_2` публикует с `(0, 1)`;
- `Printer Model` в этом mapping не участвует.

Проверка:
- <https://github.com/spuder/OpenSpool/blob/main/firmware/conf.d/pn532_rfid-ams.yaml#L1-L167>

Рекомендация для `iDryer`:

- если понадобится AMS-like режим, mapping `reader/unit -> amsId/trayId` задавать явно;
- не строить mapping из `Printer Model`.

## 13. Как строится Bambu payload в OpenSpool

Факты:

- `generate_mqtt_payload(openspool_tag_json, ams_id, ams_tray)`:
  - валидирует JSON;
  - проверяет `version == "1.0"`;
  - требует поля `color_hex`, `min_temp`, `max_temp`, `brand`, `type`;
  - принимает `color_hex` длиной только `6` или `8`;
  - формирует:
    - `sequence_id = "0"`
    - `command = "ams_filament_setting"`
    - `ams_id`
    - `tray_id`
    - `tray_color`
    - `nozzle_temp_min`
    - `nozzle_temp_max`
    - `tray_type`
    - `setting_id = ""`
    - `tray_info_idx = get_bambu_code(type, brand)`

Проверка:
- <https://github.com/spuder/OpenSpool/blob/main/firmware/bambu.h#L96-L163>

Рекомендация для `iDryer`:

- builder на `LINK` должен повторять этот контракт;
- `setting_id` сейчас отправлять пустой строкой;
- `tray_info_idx` вычислять из той же mapping-таблицы, что использует `OpenSpool`.

## 14. Роли компонентов в iDryer

### RP2040

Рекомендация:

- оставить только RFID transport layer;
- не хранить Bambu credentials;
- не строить Bambu MQTT payload;
- передавать в `LINK`:
  - raw tag payload
  - reader/unit id
  - события detect/remove

### LINK

Рекомендация:

- хранить Bambu config и MQTT session;
- валидировать tag payload;
- нормализовать цвет в RGBA;
- вычислять `tray_info_idx`;
- публиковать `ams_filament_setting`;
- поддерживать два входа в один publish path:
  - auto apply по RFID
  - manual `Test apply` из frontend

### Backend / Frontend

Рекомендация:

- использовать уже существующий `LINK/Bambu` раздел для credentials;
- добавить туда runtime status `MQTT Connection / Bambu status`;
- добавить туда `Test apply`;
- не выносить Bambu-настройки в редактор спула для MVP.

## 15. Конкретный чеклист для iDryer

### LINK

- [ ] Хранить один активный Bambu config: `model`, `ip`, `serial`, `lan_access_code`
- [ ] Реализовать MQTT client с параметрами `OpenSpool`: `8883`, `bblp`, TLS, `device/<serial>/request`
- [ ] Повторить connect gating по `IP + Serial + Lan Access Code`
- [ ] Реализовать status `MQTT Connection / Bambu status`
- [ ] Реализовать единый publish path для auto RFID и manual `Test apply`
- [ ] Реализовать OpenSpool-compatible builder для `ams_filament_setting`
- [ ] Нормализовать цвет до RGBA
- [ ] Вычислять `tray_info_idx` по таблице `OpenSpool`
- [ ] Отправлять `setting_id = ""`

### Backend

- [ ] Не хранить Bambu credentials
- [ ] Не дублировать mapping-логику независимо от `LINK`
- [ ] Если нужен server-side builder, держать его строго синхронным с таблицами `OpenSpool`

### Frontend

- [ ] Оставить credentials в существующем `LINK/Bambu` разделе
- [ ] Добавить `MQTT Connection / Bambu status`
- [ ] Добавить `Test apply`
- [ ] Добавить `Printer Model` field без runtime-логики

### RP2040

- [ ] Не добавлять Bambu credentials и Bambu MQTT
- [ ] Довести передачу прочитанного тега в `LINK`
- [ ] Довести export реальной RFID topology в `Hello`, чтобы `LINK` видел реальные reader mappings

## 16. Целевая цепочка для iDryer

Автоматический сценарий:

`RP2040 reads tag -> LINK receives OpenSpool JSON -> LINK validates payload -> LINK builds OpenSpool-compatible ams_filament_setting -> LINK publishes to device/<serial>/request`

Ручной сценарий:

`Frontend LINK/Bambu -> Test apply -> LINK builds the same payload -> LINK publishes the same ams_filament_setting`
