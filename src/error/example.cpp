// #include "error_bus.h"
// #include "error_post.h"

// // Простая отправка события из кода
// void heater_overmax(float tempC, uint8_t unit, uint32_t now_ms){
//   post_err_ex(unit, ERRSRC_HEATER, ERRSEV_CRITICAL,
//               ERRC_HEATER_OVERMAX, "Heater over max",
//               (int32_t)lroundf(tempC*10), now_ms);
// }

// // Отправка ошибки датчика (там, где читаете датчики)
// auto a = airSensors[unit]->get();  // ваш драйвер
// if (!a.ok) {
//   post_err_ex(unit, ERRSRC_AIR, ERRSEV_WARNING,
//               ERRC_SENSOR_AIR_INVALID, "Air sensor invalid",
//               (int32_t)a.err, now_ms);
// }

// // Чтение и вывод из очереди (в loop()/главном цикле)
// void pump_errors_to_serial() {
//   ErrorEvent ev;
//   while (errorbus_poll(&ev)) {
//     Serial.printf("[ERR] t=%lu ctrl=%u sev=%d src=%d code=%d data=%ld msg=%s\n",
//                   ev.ts_ms, ev.ctrl_id, ev.severity, ev.source, ev.code,
//                   (long)ev.data, ev.msg ? ev.msg : "");
//   }
// }

// // Пример с Status / ResultF
// #include "status.h"

// // Функция читает температуру и возвращает ResultF
// ResultF read_air_temp();

// void tick_air(uint8_t unit, uint32_t now_ms){
//   ResultF r = read_air_temp();
//   if (!r.ok) {
//     post_err_ex(unit, ERRSRC_AIR, ERRSEV_WARNING,
//                 r.code, r.msg, 0, now_ms);
//     return; // работайте с NAN/по месту
//   }
//   float t = r.value;
//   // ... используем t ...
// }

// //  Из прерывания (ISR) — просто вызывайте post_err_*
// void __isr my_timer_isr() {
//   // ...
//   post_err_ex(0, ERRSRC_CORE, ERRSEV_ERROR,
//               ERRC_MODE_SWITCH_FAILED, "Tick handler fault", 0, millis());
//   // ...
// }

// // Со второго ядра (multicore)
// void core1_entry() {
//   for(;;){
//     // ... что-то делаете ...
//     post_err_ex(1, ERRSRC_STORAGE, ERRSEV_WARNING,
//                 ERRC_STORAGE_HUM_TIMEOUT, "RH timeout", 0, millis());
//     sleep_ms(1000);
//   }
// }