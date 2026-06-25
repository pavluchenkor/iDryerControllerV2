// debug_log.h
#pragma once
#include <Arduino.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include "local_log_config.h"

#ifndef KASYAK_FINDER
#define KASYAK_FINDER 0
#endif
#ifndef DEBUG_LOG
#define DEBUG_LOG 0
#endif
#define DEBUG_ENABLED ((KASYAK_FINDER) || (DEBUG_LOG))

// ===== Runtime gate =====
// true  = IDLE/verbose: все уровни логов работают
// false = активный режим: INFO/DEBUG/TRACE подавляются, WARN/ERROR/CRIT/OPR всегда видны
#if DEBUG_ENABLED
extern volatile bool g_verbose_logs;
#endif

#define LOG_LEVEL_NONE      0  // no logs                       
#define LOG_LEVEL_CRITICAL  1  // critical only                  КРАСНЫЙ ЖИРНЫЙ
#define LOG_LEVEL_ERROR     2  // errors only                    КРАСНЫЙ
#define LOG_LEVEL_WARN      3  // warnings + errors              ЖЁЛТЫЙ  
#define LOG_LEVEL_INFO      4  // info + warn + error            СИНИЙ
#define LOG_LEVEL_DEBUG     5  // debug + info + warn + error    МАГЕНТА
#define LOG_LEVEL_TRACE     6  // everything                     СЕРЫЙ-БЕЛЫЙ



#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_INFO
#endif

#ifndef LOG_BUFFER_SIZE
// малый стек — сначала пытаемся в стеке, если не влезло — аллоцируем на куче
#define LOG_BUFFER_SIZE 384
#endif

// ===== ANSI colors =====
#define ANSI_RESET     "\033[0m"
#define ANSI_BOLD      "\033[1m"
#define ANSI_DIM       "\033[2m"
#define ANSI_UNDERLINE "\033[4m"

#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_BLUE    "\033[34m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_WHITE   "\033[37m"

#define LOG_COLOR_CRIT   ANSI_BOLD ANSI_RED
#define LOG_COLOR_ERROR  ANSI_RED
#define LOG_COLOR_WARN   ANSI_YELLOW
#define LOG_COLOR_INFO   ANSI_BLUE
#define LOG_COLOR_DEBUG  ANSI_MAGENTA
#define LOG_COLOR_TRACE  ANSI_DIM ANSI_WHITE
#define LOG_COLOR_OK     ANSI_GREEN

// ===== Пер-файловый тег по умолчанию =====
static inline const char* __log_file_tag__() {
  const char* p = strrchr(__FILE__, '/');
#ifdef _WIN32
  const char* q = strrchr(__FILE__, '\\');
  if (!p || (q && q > p)) p = q;
#endif
  return p ? p + 1 : __FILE__;
}
#ifndef LOG_TAG
  #define LOG_TAG  __log_file_tag__()
#endif

// (опционально) таймштамп в миллисекундах
#ifndef LOG_SHOW_TIME_MS
#define LOG_SHOW_TIME_MS 0   // 1 = печатать "12345ms " перед сообщениями
#endif

// ===== НИЗКОУРОВНЕВАЯ БЕЗОПАСНАЯ ПЕЧАТЬ =====
#if DEBUG_ENABLED
// Печать форматированной строки c ограничением размера и fallback на heap, если не влезло
static inline void __dbg_vprint_fmt(const char* fmt, va_list ap) {
  // 1) посчитаем требуемую длину (нужна копия va_list!)
  va_list ap_len;
  va_copy(ap_len, ap);
  int need = vsnprintf(nullptr, 0, fmt, ap_len);
  va_end(ap_len);
  if (need < 0) return;

  if (need < (int)LOG_BUFFER_SIZE) {
    // 2а) влезает в стековый буфер
    char buf[LOG_BUFFER_SIZE];
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    Serial.print(buf);
  } else {
    // 2б) не влезает аллоцируем
    char* big = (char*)malloc((size_t)need + 1);
    if (!big) {
      // крайний случай: печатаем усечённо
      char buf[LOG_BUFFER_SIZE];
      (void)vsnprintf(buf, sizeof(buf), fmt, ap);
      Serial.print(buf);
      return;
    }
    (void)vsnprintf(big, (size_t)need + 1, fmt, ap);
    Serial.write((const uint8_t*)big, (size_t)need);
    free(big);
  }
}

static inline void __dbg_log_core(const char* color, const char* lvl,
                                  const char* tag,
                                  const char* fmt, va_list ap) {
  if (LOG_SHOW_TIME_MS) {
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "%lums ", (unsigned long)millis());
    Serial.print(tbuf);
  }
  Serial.print(color);
  Serial.print('['); Serial.print(lvl); Serial.print(']');
  Serial.print('['); Serial.print(tag); Serial.print(']');
  Serial.print(' ');

  __dbg_vprint_fmt(fmt, ap);

  Serial.print(ANSI_RESET);
  Serial.print('\n');
}

// Вариативная обёртка (чтобы макросы не трогали va_list/va_start)
static inline void __dbg_logf(const char* color, const char* lvl,
                              const char* tag, const char* fmt, ...) {
  va_list ap; va_start(ap, fmt);
  __dbg_log_core(color, lvl, tag, fmt, ap);
  va_end(ap);
}
#endif // DEBUG_ENABLED

// ===== Базовые макросы =====
#if DEBUG_ENABLED
  // оставляем как есть для совместимости
  #define DEBUG_PRINTF(fmt, ...) \
      do { Serial.printf((fmt), ##__VA_ARGS__); } while (0)

  // Единая точка форматирования: цвет + [LVL][TAG] + msg
  // Переводим на безопасную __dbg_logf (без va_start в макросах!)
  #define __DBG_PRINT(color, lvl, fmt, ...)                                      \
    do { __dbg_logf((color), (lvl), (LOG_TAG), (fmt), ##__VA_ARGS__); } while (0)
#else
  #define DEBUG_PRINTF(...)   do {} while (0)
  #define __DBG_PRINT(...)    do {} while (0)
#endif

// ===== Макросы по уровням (полные + короткие алиасы) =====
#if LOG_LEVEL >= LOG_LEVEL_CRITICAL
  #define DEBUG_CRIT(fmt, ...) __DBG_PRINT(LOG_COLOR_CRIT, "CRT", fmt, ##__VA_ARGS__)
  #define DEBUG_C(...)          DEBUG_CRIT(__VA_ARGS__)
#else
  #define DEBUG_CRIT(...) do {} while (0)
  #define DEBUG_C(...)     do {} while (0)
#endif


#if LOG_LEVEL >= LOG_LEVEL_ERROR
  #define DEBUG_ERROR(fmt, ...) __DBG_PRINT(LOG_COLOR_ERROR, "ERR", fmt, ##__VA_ARGS__)
  #define DEBUG_E(...)          DEBUG_ERROR(__VA_ARGS__)
#else
  #define DEBUG_ERROR(...) do {} while (0)
  #define DEBUG_E(...)     do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
  #define DEBUG_WARN(fmt, ...)  __DBG_PRINT(LOG_COLOR_WARN,  "WRN", fmt, ##__VA_ARGS__)
  #define DEBUG_W(...)          DEBUG_WARN(__VA_ARGS__)
#else
  #define DEBUG_WARN(...) do {} while (0)
  #define DEBUG_W(...)    do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
  #define DEBUG_INFO(fmt, ...)  do { if (g_verbose_logs) __DBG_PRINT(LOG_COLOR_INFO,  "INF", fmt, ##__VA_ARGS__); } while(0)
  #define DEBUG_I(...)          DEBUG_INFO(__VA_ARGS__)
#else
  #define DEBUG_INFO(...) do {} while (0)
  #define DEBUG_I(...)    do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
  #define DEBUG_DEBUG(fmt, ...) do { if (g_verbose_logs) __DBG_PRINT(LOG_COLOR_DEBUG, "DBG", fmt, ##__VA_ARGS__); } while(0)
  #define DEBUG_D(...)          DEBUG_DEBUG(__VA_ARGS__)
#else
  #define DEBUG_DEBUG(...) do {} while (0)
  #define DEBUG_D(...)     do {} while (0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_TRACE
  #define DEBUG_TRACE(fmt, ...) do { if (g_verbose_logs) __DBG_PRINT(LOG_COLOR_TRACE, "TRC", fmt, ##__VA_ARGS__); } while(0)
  #define DEBUG_T(...)          DEBUG_TRACE(__VA_ARGS__)
#else
  #define DEBUG_TRACE(...) do {} while (0)
  #define DEBUG_T(...)     do {} while (0)
#endif

// ===== Operational log: обходит гейт, всегда виден в активном режиме =====
// Использовать для: температура, PID-статус, ключевые рабочие события
#if DEBUG_ENABLED
  #define DEBUG_OP(fmt, ...) __DBG_PRINT(LOG_COLOR_OK,  "OPR", fmt, ##__VA_ARGS__)
  #define DEBUG_O(...)       DEBUG_OP(__VA_ARGS__)
#else
  #define DEBUG_OP(...) do {} while (0)
  #define DEBUG_O(...)  do {} while (0)
#endif