## Телеметрия при аварии / датчик «нет данных» (в работе)

- **Троттлинг/дедуп отправки событий errbus (ПРИОРИТЕТ 1).**
  При обрыве/замыкании датчика `SENSOR_INVALID` постится каждую итерацию `loop()` (~5 мс) — троттла нет ни в `post_err_ex`, ни в `errorbus_post`. Спам забивает UART (`Local error code=5`) и MQTT (14 events/сек), дёргает `setMode(Idle)` сотни раз/сек, вероятно роняет линк в offline.
  Дедуп-механика уже есть в `errlog_append_from_event` (сравнение `(code,source,ctrl_id)` + инкремент `count`), но НЕ применяется к отправке `sendUartErrorEvent`. Применить: одинаковое `(src,code,ctrl_id)` слать не чаще раза в N секунд, первое — сразу.

- **Sentinel «нет данных» в телеметрии (ПРИОРИТЕТ 2, после троттлинга).**
  Sentinel = максимум типа поля в домене C10 (`INT16_MAX`/`UINT16_MAX`), только для датчиковых полей (`temperatureC10`, `humidityPct10`, опц. `weightGramsC10`). `targetTempC10` НЕ трогаем (setpoint, не датчик).
  Логика в генераторе `gen_uart_protocol_h.py` (`render_scaling_accessors`): `setX` → sentinel при `isnan`, `getX` → `NAN` при sentinel; `#include <math.h>`; warning-комментарий над raw-полем. Затем `regen.sh`, перевод RP/ESP с прямого доступа к полям на `setX/getX`. На RP убрать заморозку `inputs_` ([main.cpp:1937]) — иначе `isnan` не сработает.
  Портал: миграция `temperature`/`humidity` → nullable, `telemetry.handler.ts` писать `null`-точку вместо пропуска, `DeviceDetailPage.tsx` убрать `connectNulls`.

- **Рефакторинг источника данных дисплея.**
  `service_screen` делает свой `readDryerInputs()` со side-effects (`report_sensor_error` → POST_ERROR) — удваивает спам событий. Брать готовый снимок из `inputs_`/кэша, а `readDryerInputs` вызывать один раз за цикл.

- Устройство ушло в offline{} и переподключилось. Наиболее вероятная причина — спам по ошибке перегрузил UART/MQTT, сработал keepalive-timeout или watchdog → reconnect. Проверить. 


- клайм через меню
- Разобраться с версией совместимости Link <-> RP2040.
  Сейчас `VERSION_MAJOR` используется как признак совместимости UART/прошивок и одновременно связан с обновлением меню. Нужно решить, где должна жить версия контракта: в firmware major, в UART/protocol contract yaml или отдельно в idryer-core.


❌ Осталось (из TODO)
Пункт 3 — рефакторинг источника данных дисплея. service_screen делает свой readDryerInputs() со side-effects (report_sensor_error) — удваивает спам событий. Связан с троттлингом, но не закрыт.


Версия совместимости Link↔RP2040 — не трогали. И стало актуальнее: мы меняли wire-формат telemetry payload (29→37 байт), major не поднимали — старая↔новая прошивки по этому payload несовместимы. Стоит вписать в этот пункт.


❌ Не закоммичено ❌
Портальная sentinel-часть (nullable + миграция + connectNulls + applyMovingAverage-фикс)
heaterTemp целиком (прошивка idryer-core/RP/ESP + портал)