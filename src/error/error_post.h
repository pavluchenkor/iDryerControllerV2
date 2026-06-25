#pragma once
#include "error_bus.h"
#include <Arduino.h>

/*** ERROR LOG FORMAT CONVENTION ***
* [severity][unit_id] 
* src:source_of_error
* code:error_code  
* msg:error_message
* data:data_at_the_time_of_the_error  
* t:time_of_the_error
************************************/

/**
 * @brief Постинг события ошибки (полная версия).
 *
 * Формирует структуру ErrorEvent и отправляет её в шину ошибок.
 *
 * @param ctrl_id  Идентификатор контроллера .
 * @param src      Источник ошибки (ErrSource).
 * @param sev      Важность/критичность (ErrSeverity).
 * @param code     Код ошибки (ErrCode).
 * @param msg      Короткое сообщение (читаемый заголовок). Должно жить до обработки
 *                 (желательно строковый литерал или статическая строка).
 * @param data     Доп. числовые данные (контекст), например измеренное значение.
 * @param now_ms   Таймстамп в миллисекундах (обычно millis()).
 * @return true, если событие принято шиной ошибок; false — если очередь переполнена/отклонено.
 * @sa errorbus_post, POST_ERROR, post_err
 */
static inline bool post_err_ex(uint8_t ctrl_id,
                               ErrSource src,
                               ErrSeverity sev,
                               ErrCode code,
                               const char* msg,
                               int32_t data,
                               uint32_t now_ms)
{
  ErrorEvent e = { now_ms, sev, src, code, msg, data, ctrl_id };
  return errorbus_post(&e);
}

/**
 * @brief Постинг события ошибки (короткая версия).
 *
 * Важность выводится автоматически из кода: ERRC_OK → INFO, иначе → WARNING.
 * ctrl_id фиксирован в 0.
 *
 * @param src     Источник ошибки (ErrSource).
 * @param code    Код ошибки (ErrCode).
 * @param msg     Короткое сообщение (см. требования к жизни строки).
 * @param data    Доп. числовые данные (контекст).
 * @param now_ms  Таймстамп в миллисекундах (обычно millis()).
 * @return void
 * @sa post_err_ex
 */
static inline void post_err(ErrSource src,
                            ErrCode code,
                            const char* msg,
                            int32_t data,
                            uint32_t now_ms)
{
  ErrSeverity sev = (code == ERRC_OK) ? ERRSEV_INFO : ERRSEV_WARNING;
  post_err_ex(0, src, sev, code, msg, data, now_ms);
}

/**
 * @brief Упрощённый макрос-функция для постинга ошибки с автоподстановкой времени.
 *
 * Обёртка над post_err_ex(...) с now_ms = millis().
 *
 * @param sev      Важность/критичность (ErrSeverity).
 * @param ctrl_id  Идентификатор контроллера .
 * @param src      Источник ошибки (ErrSource).
 * @param code     Код ошибки (ErrCode).
 * @param msg      Короткое сообщение (см. требования к жизни строки).
 * @param data     Доп. числовые данные (контекст).
 * @return true, если событие принято шиной; false — если отклонено.
 * @sa post_err_ex
 */
static inline bool POST_ERROR(ErrSeverity sev,
                              uint8_t ctrl_id,
                              ErrSource src,
                              ErrCode code,
                              const char* msg,
                              int32_t data)
{
  return post_err_ex(ctrl_id, src, sev, code, msg, data, millis());
}


/** INFO */
static inline bool post_info(uint8_t ctrl_id,
                             ErrSource src,
                             ErrCode code,
                             const char* msg,
                             int32_t data)
{
  return post_err_ex(ctrl_id, src, ERRSEV_INFO, code, msg, data, millis());
}

/** WARN */
static inline bool post_warn(uint8_t ctrl_id,
                             ErrSource src,
                             ErrCode code,
                             const char* msg,
                             int32_t data)
{
  return post_err_ex(ctrl_id, src, ERRSEV_WARNING, code, msg, data, millis());
}

/** ERROR */
static inline bool post_error(uint8_t ctrl_id,
                              ErrSource src,
                              ErrCode code,
                              const char* msg,
                              int32_t data)
{
  return post_err_ex(ctrl_id, src, ERRSEV_ERROR, code, msg, data, millis());
}

/** CRITICAL */
static inline bool post_critical(uint8_t ctrl_id,
                                 ErrSource src,
                                 ErrCode code,
                                 const char* msg,
                                 int32_t data)
{
  return post_err_ex(ctrl_id, src, ERRSEV_CRITICAL, code, msg, data, millis());
}























// // Полная версия (явная важность + ctrl_id)
// static inline bool post_err_ex(uint8_t ctrl_id,
//                                ErrSource src,
//                                ErrSeverity sev,
//                                ErrCode code,
//                                const char* msg,
//                                int32_t data,
//                                uint32_t now_ms)
// {
//   ErrorEvent e = { now_ms, sev, src, code, msg, data, ctrl_id };
//   // Serial.println("------------------------------ >Posting error");
//   return errorbus_post(&e);
// }

// // Короткая версия (важность по коду; ctrl_id=0)
// static inline void post_err(ErrSource src,
//                             ErrCode code,
//                             const char* msg,
//                             int32_t data,
//                             uint32_t now_ms)
// {
//   ErrSeverity sev = (code == ERRC_OK) ? ERRSEV_INFO : ERRSEV_WARNING;
//   post_err_ex(0, src, sev, code, msg, data, now_ms);
// }

// static inline bool POST_ERROR(uint8_t ctrl_id,
//                                ErrSeverity sev,
//                                ErrSource src,
//                                ErrCode code,
//                                const char* msg,
//                                int32_t data){
// return post_err_ex(ctrl_id, src, sev, code, msg, data, millis());
// }