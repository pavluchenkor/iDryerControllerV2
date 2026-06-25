TODO: 
нужно написать:

  [ ] перейти на субмодуль
  [ ] исправить enum class DryerState : uint8_t
  [ ] исправить FaultCode : uint8_t
  [ ] menu_to_json.cpp - конвертирует g_menu[] → JSON
  [ ] menu_to_json.h - заголовок
  [ ] Добавить логику фрагментации при отправке больших JSON
  [ ] Добавить логику сборки фрагментов на ESP32


# JSON Link Protocol: Передача конфигурации меню

## Обзор

Протокол передачи полной структуры меню от RP2040 к ESP32 (Link) для отображения на экране или в веб-интерфейсе.

**Ключевой принцип:** Link — "тупая труба". Не знает структуру меню, просто принимает JSON и ретранслирует.

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│     RP2040      │  UART   │   ESP32 Link    │  MQTT/  │  Backend/UI     │
│  (источник      │ ──────► │  (транспорт)    │  WS     │  (потребитель)  │
│   истины)       │ ◄────── │                 │ ──────► │                 │
└─────────────────┘         └─────────────────┘         └─────────────────┘
     │                            │                           │
     │ Знает:                     │ Знает:                    │ Знает:
     │ - Структуру меню           │ - Как передать JSON       │ - Как отрисовать
     │ - Бизнес-логику            │ - UART ↔ MQTT/WS          │ - Как редактировать
     │ - EEPROM                   │                           │
     │                            │                           │
     │ НЕ знает:                  │ НЕ знает:                 │ НЕ знает:
     │ - WiFi/MQTT                │ - Что такое "dry_temp"    │ - EEPROM адреса
     │                            │ - Структуру меню          │ - Внутреннюю логику
```

---

## Формат JSON

### Полная структура меню

Передаётся при:
- Старте устройства
- Запросе `get_config`
- Смене языка (отправляется заново с новым языком)

**ВАЖНО:** Для экономии трафика передаётся только **текущий язык** (указанный в поле `lang`).
- `title` и `unit` — строки на текущем языке, а не объекты `{"ru":"...", "en":"..."}`
- При смене языка отправляется весь конфиг заново, но с новым языком
- Экономия размера: ~4-5 KB вместо ~8 KB (все языки)

```json
{
  "v": 8,
  "units": 3,
  "active": 0,
  "lang": "ru",
  "menu": [
    {
      "id": 0,
      "title": "МЕНЮ",
      "t": "sub",
      "p": -1,
      "ch": [1, 2, 6, 10, 129, 161, 184]
    },
    {
      "id": 3,
      "title": "ТЕМПЕРАТУРА",
      "unit": "°C",
      "t": "val",
      "p": 2,
      "vt": "f32",
      "min": 30,
      "max": 110,
      "step": 1,
      "scope": "u",
      "val": [50.0, 55.0, 60.0]
    },
    {
      "id": 5,
      "title": "СТАРТ",
      "t": "act",
      "p": 2
    }
  ]
}
```

### Описание полей

#### Корневой объект

| Поле | Тип | Описание |
|------|-----|----------|
| `v` | uint | Версия конфигурации (EE_VERSION) |
| `units` | uint | Количество юнитов (NUM_UNITS) |
| `active` | uint | Активный юнит (0..units-1) |
| `lang` | string | Текущий язык ("ru", "en") |
| `menu` | array | Массив пунктов меню |

#### Пункт меню (MenuItem)

| Поле | Тип | Обязательно | Описание |
|------|-----|-------------|----------|
| `id` | uint | да | Уникальный ID пункта (индекс в g_menu[]) |
| `title` | string | да | Название на текущем языке (указанном в `lang`) |
| `t` | string | да | Тип: `"sub"`, `"val"`, `"tog"`, `"act"` |
| `p` | int | да | Parent ID (-1 для корня) |
| `ch` | array | нет | Children IDs (только для submenu) |
| `unit` | string | нет | Единица измерения на текущем языке |
| `vt` | string | нет | Value type: `"f32"`, `"u16"`, `"u8"`, `"i32"`, `"bool"` |
| `min` | number | нет | Минимальное значение |
| `max` | number | нет | Максимальное значение |
| `step` | number | нет | Шаг изменения |
| `scope` | string | нет | `"g"` = global, `"u"` = per_unit |
| `val` | any | нет | Значение: число, bool, или массив `[v0, v1, v2]` |

#### Типы пунктов меню

| Тип | Код | Описание | Поля |
|-----|-----|----------|------|
| Submenu | `"sub"` | Контейнер | `ch` (children) |
| Value | `"val"` | Редактируемое значение | `vt`, `min`, `max`, `step`, `scope`, `val` |
| Toggle | `"tog"` | Переключатель bool | `scope`, `val` |
| Action | `"act"` | Кнопка-действие | — |

#### Scope (область видимости)

| Код | Описание | Формат val |
|-----|----------|------------|
| `"g"` | Global — одно значение на всё устройство | `"val": 50.0` |
| `"u"` | Per-unit — отдельное значение для каждого юнита | `"val": [50.0, 55.0, 60.0]` |

---

## Delta-обновления

**Delta** (от греческой буквы Δ "дельта" = изменение, разность) — частичное обновление вместо полного.

**Зачем:** Экономия трафика — отправляем только изменённые настройки вместо всего меню.

**Экономия:**
- Полный конфиг: ~4500 байт
- Delta (1-3 изменения): ~50 байт
- **В 90 раз меньше!**

### Формат Delta

Передаются **только ID** (не bind-имена):

```json
{
  "d": {
    "3": [51.0, 55.0, 60.0],
    "4": [239, 180, 300]
  }
}
```

**Ключи:** ID настроек (числа как строки)

**Значения:**
- Для `scope: "g"` → одно значение: `"1": 0`
- Для `scope: "u"` → массив: `"3": [51.0, 55.0, 60.0]`

### Когда отправляется Delta

**От RP2040 → ESP32 → MQTT:**
- После изменения настройки (пользователь, команда, автоматика)
- Периодически для динамических значений (elapsed_time, current_temp)

**Обработка ESP32:**
1. Получает delta от RP2040
2. Обновляет свою копию menu[] по ID
3. Публикует delta в MQTT для синхронизации с Portal/App

---

## Команды редактирования

### Установка значения

От UI к RP2040 через Link:

**Обязательные поля:**
- `cmd`: "set"
- `id`: номер настройки (основной способ) или `bind`: имя (вспомогательный)
- `val`: новое значение

**Опциональные поля:**
- `unit`: номер юнита (0, 1, 2, ...)

**Правила параметра `unit`:**

| scope | unit нужен? | Пример |
|-------|-------------|--------|
| `"g"` (global) | ❌ НЕТ | `{"cmd": "set", "id": 1, "val": 0}` |
| `"u"` (per-unit) | ✅ ДА | `{"cmd": "set", "id": 3, "unit": 0, "val": 55.0}` |

**Примеры:**

**Global (язык, активный юнит):**
```json
{"cmd": "set", "id": 212, "val": 0}
```

**Per-unit (температура юнита 1):**
```json
{"cmd": "set", "id": 3, "unit": 1, "val": 55.0}
```

**По bind-имени (для отладки):**
```json
{"cmd": "set", "bind": "dry_temp", "unit": 0, "val": 55.0}
```

**Приоритет:** Основной способ — **по id** (всегда доступен, быстрее). bind — опционально для удобства отладки.

### Вызов action

```json
{"cmd": "invoke", "id": 5}
```

или

```json
{"cmd": "invoke", "action": "start_drying"}
```

### Запрос конфига

```json
{"cmd": "get_config"}
```

### Смена активного юнита

```json
{"cmd": "set_active", "unit": 1}
```

---

## Реализация на RP2040

### Сериализатор (menu_to_json.cpp)

```cpp
#include <ArduinoJson.h>
#include "menu_types.h"
#include "menu_state.h"
#include "menu_bindings.h"

/**
 * Сериализует полную структуру меню в JSON
 * @param buf      Буфер для JSON
 * @param buf_size Размер буфера (по умолчанию 8192)
 * @param lang_id  ID языка (LANG_RU, LANG_EN)
 * @return         Количество записанных байт
 */
size_t menu_to_json(char* buf, size_t buf_size, uint8_t lang_id) {
    JsonDocument doc;

    // Метаданные
    doc["v"] = EE_VERSION;
    doc["units"] = NUM_UNITS;
    doc["active"] = menu_get_active_controller();
    doc["lang"] = (lang_id == LANG_RU) ? "ru" : "en";

    JsonArray items = doc["menu"].to<JsonArray>();

    for (uint16_t i = 0; i < MENU__COUNT; i++) {
        const MenuItem& m = g_menu[i];
        JsonObject item = items.add<JsonObject>();

        item["id"] = i;

        // Title (все языки)
        JsonObject title = item["title"].to<JsonObject>();
        for (uint8_t l = 0; l < LANG_COUNT; l++) {
            const char* lang_code = (l == LANG_RU) ? "ru" : "en";
            if (m.title[l]) title[lang_code] = m.title[l];
        }

        // Unit (если есть)
        if (m.unit[0] || m.unit[1]) {
            JsonObject unit = item["unit"].to<JsonObject>();
            for (uint8_t l = 0; l < LANG_COUNT; l++) {
                const char* lang_code = (l == LANG_RU) ? "ru" : "en";
                if (m.unit[l]) unit[lang_code] = m.unit[l];
            }
        }

        // Тип
        static const char* type_codes[] = {"sub", "act", "val", "tog"};
        item["t"] = type_codes[m.type];

        // Иерархия
        item["p"] = m.parent;

        if (m.type == MN_SUBMENU && m.child_count > 0) {
            JsonArray ch = item["ch"].to<JsonArray>();
            for (uint16_t c = 0; c < m.child_count; c++) {
                ch.add(m.first_child + c);
            }
        }

        // Value/Toggle специфика
        if (m.type == MN_VALUE || m.type == MN_TOGGLE) {
            const ValueSpec& vs = m.u.value;

            // Тип значения
            static const char* vtype_codes[] = {"f32", "u16", "u8", "i32", "bool", "u32"};
            item["vt"] = vtype_codes[vs.vtype];

            // Границы (только для value)
            if (m.type == MN_VALUE) {
                item["min"] = vs.minv;
                item["max"] = vs.maxv;
                item["step"] = vs.step;
            }

            // Scope и значения
            const MenuBinding* b = menu_find_bind_by_ptr(vs.ptr);
            if (b && b->scope == SCOPE_PER_CONTROLLER) {
                item["scope"] = "u";
                JsonArray vals = item["val"].to<JsonArray>();
                for (uint8_t u = 0; u < NUM_UNITS; u++) {
                    vals.add(read_value_at(vs, u));
                }
            } else {
                item["scope"] = "g";
                item["val"] = read_value_at(vs, 0);
            }
        }
    }

    return serializeJson(doc, buf, buf_size);
}

/**
 * Сериализует только изменённые значения (delta)
 */
size_t menu_delta_json(char* buf, size_t buf_size, uint16_t* changed_ids, uint8_t count) {
    JsonDocument doc;
    JsonObject d = doc["d"].to<JsonObject>();

    for (uint8_t i = 0; i < count; i++) {
        uint16_t id = changed_ids[i];
        const MenuItem& m = g_menu[id];
        if (m.type != MN_VALUE && m.type != MN_TOGGLE) continue;

        const ValueSpec& vs = m.u.value;
        const MenuBinding* b = menu_find_bind_by_ptr(vs.ptr);

        char key[8];
        snprintf(key, sizeof(key), "%u", id);

        if (b && b->scope == SCOPE_PER_CONTROLLER) {
            JsonArray vals = d[key].to<JsonArray>();
            for (uint8_t u = 0; u < NUM_UNITS; u++) {
                vals.add(read_value_at(vs, u));
            }
        } else {
            d[key] = read_value_at(vs, 0);
        }
    }

    return serializeJson(doc, buf, buf_size);
}

// Вспомогательная функция
static float read_value_at(const ValueSpec& vs, uint8_t idx) {
    switch (vs.vtype) {
        case VT_F32:  return ((float*)vs.ptr)[idx];
        case VT_U16:  return (float)((uint16_t*)vs.ptr)[idx];
        case VT_U8:   return (float)((uint8_t*)vs.ptr)[idx];
        case VT_I32:  return (float)((int32_t*)vs.ptr)[idx];
        case VT_BOOL: return ((bool*)vs.ptr)[idx] ? 1.0f : 0.0f;
        case VT_U32:  return (float)((uint32_t*)vs.ptr)[idx];
        default:      return 0.0f;
    }
}
```

---

## Реализация на ESP32 Link

### Получение и ретрансляция

```cpp
// Link — просто передаёт JSON без парсинга структуры

void onUartConfigReceived(const uint8_t* data, size_t len) {
    // Получили JSON от RP2040 → отправляем на сервер
    mqtt.publish("idryer/{serial}/config", data, len);

    // Или на локальный экран
    display.updateFromJson((const char*)data, len);
}

void onMqttSetConfig(const char* payload, size_t len) {
    // Получили команду от сервера → отправляем на RP2040
    uart.sendFrame(MSG_SET_CONFIG, payload, len);
}
```

### Для экрана (если на ESP32)

```cpp
class MenuRenderer {
    JsonDocument menuDoc;  // Кэш структуры меню

public:
    void updateFromJson(const char* json, size_t len) {
        deserializeJson(menuDoc, json, len);
        redraw();
    }

    void redraw() {
        const char* lang = "ru";  // или из настроек

        // Отрисовка текущего submenu
        JsonArray menu = menuDoc["menu"];
        int currentId = getCurrentMenuId();

        for (JsonObject item : menu) {
            if (item["p"].as<int>() == currentId) {
                const char* title = item["title"][lang];
                const char* type = item["t"];

                if (strcmp(type, "sub") == 0) {
                    drawSubmenuItem(title);
                } else if (strcmp(type, "val") == 0) {
                    float val = getDisplayValue(item);
                    const char* unit = item["unit"][lang] | "";
                    drawValueItem(title, val, unit);
                } else if (strcmp(type, "tog") == 0) {
                    bool val = getDisplayValue(item) != 0;
                    drawToggleItem(title, val);
                } else if (strcmp(type, "act") == 0) {
                    drawActionItem(title);
                }
            }
        }
    }

    float getDisplayValue(JsonObject& item) {
        uint8_t activeUnit = menuDoc["active"];
        JsonVariant val = item["val"];

        if (val.is<JsonArray>()) {
            return val[activeUnit].as<float>();
        }
        return val.as<float>();
    }
};
```

---

## UART протокол

### Типы сообщений

| Код | Направление | Описание |
|-----|-------------|----------|
| `0x20` | RP2040 → ESP32 | CONFIG_FULL — полный JSON |
| `0x21` | RP2040 → ESP32 | CONFIG_DELTA — изменения |
| `0x22` | ESP32 → RP2040 | CONFIG_SET — установка значения |
| `0x23` | ESP32 → RP2040 | CONFIG_GET — запрос конфига |
| `0x24` | ESP32 → RP2040 | ACTION_INVOKE — вызов action |

### Формат кадра (базовый протокол)

```
┌──────┬─────┬───────┬──────┬─────┬────────┬──────────────┬──────┬──────┐
│ SOF  │ Ver │ Flags │ Kind │ Seq │ Length │ JSON Payload │ CRC  │ CRC  │
│ 0xAA │ 0x01│ 1B    │ 1B   │ 1B  │ 1B     │ N bytes      │ Low  │ High │
└──────┴─────┴───────┴──────┴─────┴────────┴──────────────┴──────┴──────┘
```

**Поля:**
- `SOF`: Стартовый байт (0xAA)
- `Ver`: Версия протокола (0x01)
- `Flags`: Флаги (ACK, фрагментация)
- `Kind`: Тип сообщения (0x30 = CONFIG_FULL, 0x31 = CONFIG_DELTA, и т.д.)
- `Seq`: Номер последовательности (0-255)
- `Length`: Длина payload (0-255 байт)
- `Payload`: JSON данные (или фрагмент)
- `CRC16`: Контрольная сумма (младший, старший байт)

### Фрагментация больших JSON

**Проблема:** Полный конфиг меню ~4-5 KB, а MAX_PAYLOAD_SIZE = 200 байт.

**Решение:** Байтовая фрагментация — разбить JSON на куски по 200 байт.

#### Флаги фрагментации

```cpp
FLAG_FRAGMENTED      = 0x08  // Это фрагмент (не последний)
FLAG_LAST_FRAGMENT   = 0x10  // Последний фрагмент
```

**Комбинации:**
- `Flags = 0x00` — обычное сообщение (не фрагментировано)
- `Flags = 0x08` — фрагмент N (есть еще данные)
- `Flags = 0x18` — последний фрагмент (0x08 | 0x10)

#### Алгоритм передачи

**RP2040:**
```
JSON: {"v":8,"units":3,...} (4096 байт)

Фрагмент 0:
  Kind: 0x30 (CONFIG_FULL)
  Flags: 0x08 (FRAGMENTED)
  Seq: 0
  Length: 200
  Payload: [байты 0-199]

Фрагмент 1:
  Kind: 0x30
  Flags: 0x08
  Seq: 1
  Length: 200
  Payload: [байты 200-399]

...

Фрагмент 20 (последний):
  Kind: 0x30
  Flags: 0x18 (FRAGMENTED | LAST_FRAGMENT)
  Seq: 20
  Length: 96
  Payload: [байты 4000-4095]
```

**ESP32 (прием):**
```cpp
// Буфер для сборки
static uint8_t jsonBuffer[8192];
static uint16_t receivedBytes = 0;

void onUartFrame(Frame& frame) {
  if (frame.header.kind == 0x30) {  // CONFIG_FULL

    // Первый фрагмент — сброс буфера
    if (frame.header.sequence == 0) {
      receivedBytes = 0;
    }

    // Копируем данные в буфер
    memcpy(jsonBuffer + receivedBytes,
           frame.payload,
           frame.header.payloadLength);
    receivedBytes += frame.header.payloadLength;

    // Последний фрагмент?
    if (frame.header.flags & FLAG_LAST_FRAGMENT) {
      // Готово! Обрабатываем JSON
      processConfigJson((char*)jsonBuffer, receivedBytes);
      receivedBytes = 0;
    }
  }
}
```

#### Пример передачи 4096 байт

```
Фрагмент 0:  Seq=0,  Flags=0x08, Len=200, Offset=0
Фрагмент 1:  Seq=1,  Flags=0x08, Len=200, Offset=200
Фрагмент 2:  Seq=2,  Flags=0x08, Len=200, Offset=400
...
Фрагмент 19: Seq=19, Flags=0x08, Len=200, Offset=3800
Фрагмент 20: Seq=20, Flags=0x18, Len=96,  Offset=4000 ← Последний!

Итого: 21 пакет, время @ 115200 baud ≈ 1-2 секунды
```

---

## Размеры и ограничения

| Параметр | Значение | Примечание |
|----------|----------|------------|
| Макс. размер JSON | 8192 байт | MQTT лимит |
| Типичный размер полного меню | 4-5 KB | ~140 пунктов, **один язык** |
| Типичный размер delta | 50-200 байт | 1-5 значений |
| Буфер на RP2040 | 8192 байт | Статический |
| Буфер на ESP32 | 8192 байт | Динамический |
| Количество UART пакетов | ~20-25 | При MAX_PAYLOAD_SIZE=200 байт |

---

## Полный пример JSON (~140 пунктов, русский язык)

**Примечание:** Передаётся только текущий язык. При `lang: "ru"` все `title` и `unit` на русском.

```json
{
  "v": 8,
  "units": 3,
  "active": 0,
  "lang": "ru",
  "menu": [
    {"id":0, "title":"МЕНЮ", "t":"sub", "p":-1, "ch":[1,2,6,10,129,161,210]},

    {"id":1, "title":"КОНТРОЛЛЕР", "t":"val", "p":0, "vt":"u8", "min":0, "max":2, "step":1, "scope":"g", "val":0},

    {"id":2, "title":"СУШКА", "t":"sub", "p":0, "ch":[3,4,5]},
    {"id":3, "title":"ТЕМПЕРАТУРА", "unit":"°C", "t":"val", "p":2, "vt":"f32", "min":30, "max":110, "step":1, "scope":"u", "val":[50.0,55.0,60.0]},
    {"id":4, "title":"ВРЕМЯ", "unit":"мин", "t":"val", "p":2, "vt":"u16", "min":0, "max":600, "step":1, "scope":"u", "val":[240,180,300]},
    {"id":5, "title":"СТАРТ", "t":"act", "p":2},

    {"id":6, "title":"ХРАНЕНИЕ", "t":"sub", "p":0, "ch":[7,8,9,10]},
    {"id":7, "title":"ТЕМПЕРАТУРА", "unit":"°C", "t":"val", "p":6, "vt":"u8", "min":35, "max":90, "step":1, "scope":"u", "val":[45,45,45]},
    {"id":8, "title":"ВЛАЖНОСТЬ", "unit":"%RH", "t":"val", "p":6, "vt":"u8", "min":5, "max":30, "step":1, "scope":"u", "val":[12,12,12]},
    {"id":9, "title":"ПО ВЛАЖНОСТИ", "t":"tog", "p":6, "scope":"u", "val":[true,true,true]},
    {"id":10, "title":"СТАРТ", "t":"act", "p":6},

    {"id":11, "title":"ПРЕСЕТЫ", "t":"sub", "p":0, "ch":[12,15,18,21,24,27,30,33,36,39,42,45,48]},
    {"id":12, "title":"PLA", "t":"sub", "p":11, "ch":[13,14,15]},
    {"id":13, "title":"ТЕМПЕРАТУРА", "unit":"°C", "t":"val", "p":12, "vt":"f32", "min":35, "max":55, "step":1, "scope":"g", "val":45.0},
    {"id":14, "title":"ВРЕМЯ", "unit":"мин", "t":"val", "p":12, "vt":"u16", "min":0, "max":600, "step":5, "scope":"g", "val":240},
    {"id":15, "title":"СТАРТ", "t":"act", "p":12},

    {"id":129, "title":"ВЕСЫ", "t":"sub", "p":0, "ch":[130,131,140]},
    {"id":130, "title":"КОЛ-ВО МОДУЛЕЙ", "t":"val", "p":129, "vt":"u8", "min":0, "max":4, "step":1, "scope":"g", "val":2},
    {"id":131, "title":"ТАРА", "t":"sub", "p":129, "ch":[132,133,134,135]},
    {"id":132, "title":"КАТУШКА 1", "unit":"г", "t":"val", "p":131, "vt":"f32", "min":0, "max":2000, "step":1, "scope":"g", "val":250.0},
    {"id":140, "title":"КАЛИБРОВКА", "t":"sub", "p":129, "ch":[141,145,149,153]},
    {"id":141, "title":"КАТУШКА 1", "t":"sub", "p":140, "ch":[142,143]},
    {"id":142, "title":"ПРИНЯТЬ НОЛЬ", "t":"act", "p":141},
    {"id":143, "title":"ПРИНЯТЬ 1000 Г", "t":"act", "p":141},

    {"id":161, "title":"НАСТРОЙКИ", "t":"sub", "p":0, "ch":[162,170,178,184,195,200]},

    {"id":162, "title":"ХРАНЕНИЕ", "t":"sub", "p":161, "ch":[163,164,165,168]},
    {"id":163, "title":"АВТО ХРАНЕНИЕ", "t":"tog", "p":162, "scope":"u", "val":[true,true,true]},
    {"id":164, "title":"АВТО СУШКА", "t":"tog", "p":162, "scope":"u", "val":[true,true,true]},

    {"id":170, "title":"ПИД НАГРЕВАТЕЛЬ", "t":"sub", "p":161, "ch":[171,172,173,174,175]},
    {"id":171, "title":"КП", "t":"val", "p":170, "vt":"f32", "min":0, "max":1000, "step":0.1, "scope":"u", "val":[9.81,9.81,9.81]},
    {"id":172, "title":"КИ", "t":"val", "p":170, "vt":"f32", "min":0, "max":1000, "step":0.01, "scope":"u", "val":[0.084,0.084,0.084]},
    {"id":173, "title":"КД", "t":"val", "p":170, "vt":"f32", "min":0, "max":1000, "step":0.1, "scope":"u", "val":[87.825,87.825,87.825]},
    {"id":174, "title":"GAIN", "unit":"×", "t":"val", "p":170, "vt":"f32", "min":0.1, "max":20.0, "step":0.1, "scope":"u", "val":[2.0,2.0,2.0]},
    {"id":175, "title":"Автопид", "t":"act", "p":170},

    {"id":178, "title":"ПИД ВОЗДУХ", "t":"sub", "p":161, "ch":[179,180,181,182,183]},

    {"id":184, "title":"НАГРЕВ", "t":"sub", "p":161, "ch":[185,186,187,188,189]},
    {"id":185, "title":"МАКС.НАГРЕВАТЕЛЬ", "unit":"°C", "t":"val", "p":184, "vt":"f32", "min":30, "max":150, "step":1, "scope":"u", "val":[130.0,130.0,130.0]},
    {"id":186, "title":"МАКС.ТЕМП.ВОЗДУХ", "unit":"°C", "t":"val", "p":184, "vt":"f32", "min":30, "max":120, "step":1, "scope":"u", "val":[90.0,90.0,90.0]},
    {"id":187, "title":"ДЕЛЬТА", "unit":"°C", "t":"val", "p":184, "vt":"u8", "min":0, "max":45, "step":1, "scope":"u", "val":[35,35,35]},
    {"id":188, "title":"ДЕЛЬТА АБС/%", "t":"tog", "p":184, "scope":"u", "val":[false,false,false]},
    {"id":189, "title":"ПРОВЕРКА", "t":"sub", "p":184, "ch":[190,191,192,193]},

    {"id":195, "title":"ВЕНТИЛЯТОР", "t":"sub", "p":161, "ch":[196,197]},
    {"id":196, "title":"ПОРОГ T ВКЛ", "unit":"°C", "t":"val", "p":195, "vt":"f32", "min":40, "max":70, "step":0.5, "scope":"u", "val":[55.0,55.0,55.0]},
    {"id":197, "title":"ГИСТЕРЕЗИС", "unit":"°C", "t":"val", "p":195, "vt":"f32", "min":5, "max":20, "step":0.5, "scope":"u", "val":[5.0,5.0,5.0]},

    {"id":200, "title":"СЕРВО", "t":"sub", "p":161, "ch":[201,202,203,204,205]},
    {"id":201, "title":"ЗАКРЫТО УГОЛ", "unit":"°", "t":"val", "p":200, "vt":"u8", "min":0, "max":180, "step":1, "scope":"u", "val":[20,20,20]},
    {"id":202, "title":"ОТКРЫТО УГОЛ", "unit":"°", "t":"val", "p":200, "vt":"u8", "min":0, "max":180, "step":1, "scope":"u", "val":[50,50,50]},
    {"id":203, "title":"ВРЕМЯ ЗАКРЫТО", "unit":"с", "t":"val", "p":200, "vt":"u16", "min":0, "max":3600, "step":1, "scope":"u", "val":[600,600,600]},
    {"id":204, "title":"ВРЕМЯ ОТКРЫТО", "unit":"с", "t":"val", "p":200, "vt":"u16", "min":0, "max":600, "step":1, "scope":"u", "val":[30,30,30]},
    {"id":205, "title":"УМНЫЙ РЕЖИМ", "t":"tog", "p":200, "scope":"u", "val":[false,false,false]},

    {"id":210, "title":"ОБЩИЕ", "t":"sub", "p":0, "ch":[211,212]},
    {"id":211, "title":"КОЛ-ВО ЮНИТОВ", "t":"val", "p":210, "vt":"u8", "min":1, "max":3, "step":1, "scope":"g", "val":2},
    {"id":212, "title":"ЯЗЫК", "t":"val", "p":210, "vt":"u8", "min":0, "max":1, "step":1, "scope":"g", "val":0}
  ]
}
```

---

## Примеры использования

### 1. Инициализация при старте

```
ESP32 → RP2040:  {"cmd": "get_config"}
RP2040 → ESP32:  {полный JSON ~6KB}
ESP32 → MQTT:    публикация в idryer/{serial}/config
ESP32 → Display: отрисовка меню
```

### 2. Редактирование значения на экране

```
User: крутит энкодер на "ТЕМПЕРАТУРА"
Display: показывает слайдер 30..110, текущее 50
User: выбирает 55, нажимает OK
ESP32 → RP2040:  {"cmd": "set", "id": 3, "unit": 0, "val": 55.0}
RP2040:          применяет, сохраняет в EEPROM
RP2040 → ESP32:  {"d": {"3": [55.0, 55.0, 60.0]}}
ESP32 → Display: обновляет значение
ESP32 → MQTT:    публикация delta
```

### 3. Команда с бэкенда

```
Backend → MQTT:  {"cmd": "set", "bind": "dry_temp", "unit": 0, "val": 60.0}
ESP32 → RP2040:  ретрансляция JSON
RP2040:          применяет
RP2040 → ESP32:  {"d": {"3": [60.0, 55.0, 60.0]}}
ESP32 → Display: обновляет (если есть)
ESP32 → MQTT:    подтверждение
```

---

## Преимущества подхода

1. **Link не зависит от структуры меню** — одна прошивка для всех версий
2. **Добавление пунктов меню** — только перепрошивка RP2040
3. **Полная информация для UI** — title, min/max, step, scope
4. **Компактные delta-обновления** — минимум трафика
5. **Двунаправленность** — редактирование с экрана и бэкенда
6. **Локализация** — все языки в одном JSON

---

## Файлы реализации

| Файл | Платформа | Описание |
|------|-----------|----------|
| `menu_to_json.h` | RP2040 | Заголовок сериализатора |
| `menu_to_json.cpp` | RP2040 | Реализация сериализации |
| `menu_json_parser.cpp` | RP2040 | Парсер входящих команд |
| `link_config_bridge.cpp` | ESP32 | Мост UART ↔ MQTT/Display |
