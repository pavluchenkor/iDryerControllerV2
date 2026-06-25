#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "error_bus.h"
#include "error_table.h"  // для печати при чтении (имена кодов)
#include "menu/menu_eeprom.h"  

#ifdef __cplusplus
extern "C" {
#endif

// сколько записей храним
#ifndef ERRLOG_CAP
#define ERRLOG_CAP 128
#endif

// фиксированная запись в EEPROM (16 байт)
typedef struct {
  uint32_t ts_ms;      // 4
  int32_t  data;       // 4
  uint16_t code;       // 2 (ErrCode)
  uint8_t  source;     // 1 (ErrSource)
  uint8_t  severity;   // 1 (ErrSeverity)
  uint8_t  ctrl_id;    // 1
  uint8_t  count;      // 1 — сколько раз ошибка повторилась (дедупликация)
} ErrLogRec;           // 16

void errlog_init_eeprom(void);                          // инициализация/проверка
void errlog_clear(void);                                // очистить лог
void errlog_append_from_event(const ErrorEvent* ev);    // записать событие
uint16_t errlog_count(void);                            // текущий размер (<= ERRLOG_CAP)
bool errlog_read_ith_oldest(uint16_t i, ErrLogRec* out);  // 0..count-1
void errlog_dump_to_serial(void);                         // распечатать всё

void sys_shadow_init();
void sys_shadow_check(const char* where); 

#ifdef __cplusplus
} // extern "C"
#endif