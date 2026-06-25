#pragma once
#include <Arduino.h>
#include "configuration.h"

#include <U8g2lib.h>
extern U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C u8g2;
#include "hardware/port_config.h"

#include "error/error_defs.h"  // ERRSRC_LIST, ERRCODE_LIST
#include "error/error_eelog.h" // errlog_count, errlog_read_ith_oldest, ErrLogRec, errlog_clear

// ---------- справочники из X-макросов ----------
static inline const char *errsrc_short_name(ErrSource s) {
  switch (s) {
#define GEN_SRC_CASE(enum_id, short_str)                                                                                                                                                               \
  case enum_id:                                                                                                                                                                                        \
    return short_str;
    ERRSRC_LIST(GEN_SRC_CASE)
#undef GEN_SRC_CASE
  default:
    return "?";
  }
}
static inline const char *errcode_machine_name(ErrCode c) {
  switch (c) {
#define GEN_CODE_CASE(enum_id, mach, human)                                                                                                                                                            \
  case enum_id:                                                                                                                                                                                        \
    return mach;
    ERRCODE_LIST(GEN_CODE_CASE)
#undef GEN_CODE_CASE
  default:
    return "ERR_UNKNOWN";
  }
}
static inline const char *errcode_human_text(ErrCode c) {
  switch (c) {
#define GEN_CODE_CASE(enum_id, mach, human)                                                                                                                                                            \
  case enum_id:                                                                                                                                                                                        \
    return human;
    ERRCODE_LIST(GEN_CODE_CASE)
#undef GEN_CODE_CASE
  default:
    return "";
  }
}

// ---------- строковые утилиты 4×16 ----------
static inline void str_fit16(char out[17], const char *s) {
  if (!s) s = "";
  size_t n = 0;
  while (s[n] && n < 16) {
    out[n] = s[n];
    n++;
  }
  while (n < 16)
    out[n++] = ' ';
  out[16] = '\0';
}

// ---------- внутреннее состояние сеанса подтверждения ----------
// ВАЖНО: не static, чтобы все .cpp файлы видели ОДНУ переменную!
inline bool g_err_ack_active = false; // показ оверлея (C++17 inline variable)
inline int16_t g_err_view_idx = -1;   // 0..count-1 (от "самой старой")
inline uint16_t g_err_last_count = 0; // для подстраховки

// Возвращает указатель на начало "хвоста" (оставшаяся часть src).
static const char *utf8_copy_n(char *dst, size_t dst_sz, const char *src, size_t max_cp) {
  if (!dst || dst_sz == 0) return src ? src : "";
  dst[0] = '\0';
  if (!src) return "";

  size_t di = 0, cps = 0;
  while (src[0] && cps < max_cp) {
    unsigned char c = (unsigned char)src[0];
    size_t len = 1;
    if ((c & 0x80) == 0x00)
      len = 1; // 0xxxxxxx
    else if ((c & 0xE0) == 0xC0)
      len = 2; // 110xxxxx
    else if ((c & 0xF0) == 0xE0)
      len = 3; // 1110xxxx
    else if ((c & 0xF8) == 0xF0)
      len = 4; // 11110xxx
    else {     // невалидный байт — пропустим как 1
      len = 1;
    }

    if (di + len + 1 > dst_sz) break; // нет места с учётом '\0'
    for (size_t k = 0; k < len && src[k]; ++k) {
      dst[di++] = src[k];
    }
    src += len;
    cps++;
  }
  dst[di] = '\0';
  return src; // остаток
}

// Обернуть в две строки по 16 символов UTF-8
static void wrap_16x2(const char *s, char *l1, size_t l1sz, char *l2, size_t l2sz) {
  if (!s) s = "";
  const char *rest = utf8_copy_n(l1, l1sz, s, 16);
  // пропустить пробел в начале второй строки
  while (*rest == ' ')
    rest++;
  if (*rest)
    utf8_copy_n(l2, l2sz, rest, 16);
  else {
    if (l2 && l2sz) l2[0] = '\0';
  }
}

static void wrap_16x3(const char *s, char *l1, size_t l1sz, char *l2, size_t l2sz, char *l3, size_t l3sz) {
  if (!s) s = "";

  // Линия 1
  const char *rest = utf8_copy_n(l1, l1sz, s, 16);
  // Линия 2
  while (*rest == ' ')
    rest++; // скип пробелы в начале
  if (l2 && l2sz) {
    if (*rest)
      rest = utf8_copy_n(l2, l2sz, rest, 16);
    else
      l2[0] = '\0';
  }
  // Линия 3
  while (*rest == ' ')
    rest++; // снова скип пробелы
  if (l3 && l3sz) {
    if (*rest)
      utf8_copy_n(l3, l3sz, rest, 16);
    else
      l3[0] = '\0';
  }
}

#include <inttypes.h>

static inline void draw_error_entry_4x16(uint16_t idx_from_oldest) {
  if (!hasScreen()) return;
  ErrLogRec r{};
  bool have = errlog_read_ith_oldest(idx_from_oldest, &r);

  char l1[17], l2[17], l3[17], l4[17];
  if (!have) {
    str_fit16(l1, "No error entries");
    l2[0] = l3[0] = l4[0] = '\0';
  } else {
    // 1) Заголовок: U:<ctrl_id> <SRC>
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "U:%u %s", (unsigned)r.ctrl_id, errsrc_short_name((ErrSource)r.source));
    str_fit16(l1, tmp);

    // 2-4) "<ts_ms>: <human text>" → в 3 строки по 16
    char big[128];
    const char *txt = errcode_human_text((ErrCode)r.code);
    if (!txt) txt = "";
    int n = snprintf(big, sizeof(big), "%" PRIu32 ": ", (uint32_t)r.ts_ms);
    if (n < 0) n = 0;
    if ((size_t)n >= sizeof(big)) n = sizeof(big) - 1;
    snprintf(big + n, sizeof(big) - n, "%s", txt);

    wrap_16x3(big, l2, sizeof(l2), l3, sizeof(l3), l4, sizeof(l4));
  }

  u8g2.clearBuffer();
  u8g2.enableUTF8Print();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();

  const uint8_t h = u8g2.getMaxCharHeight();
  const uint8_t scr_h = u8g2.getDisplayHeight();
  int start_y = (int)scr_h - 4 * (int)h;
  if (start_y < 0) start_y = 0;

  u8g2.drawUTF8(0, start_y + 0 * h, l1);
  u8g2.drawUTF8(0, start_y + 1 * h, l2);
  u8g2.drawUTF8(0, start_y + 2 * h, l3);
  u8g2.drawUTF8(0, start_y + 3 * h, l4);
  u8g2.sendBuffer();
}

// ---------- API: начало/состояние/шаг ----------
static inline void error_ack_begin() {
  uint16_t n = errlog_count();
  g_err_last_count = n;
  if (n == 0) {
    g_err_ack_active = false;
    g_err_view_idx = -1;
    return;
  }
  g_err_ack_active = true;
  g_err_view_idx = (int16_t)(n - 1);
  draw_error_entry_4x16((uint16_t)g_err_view_idx);
}

// true, если оверлей сейчас активен
static inline bool error_ack_active() { return g_err_ack_active; }

// Обновление состояния. Возвращает true, если оверлей перехватил кадр (активен).
// Управление:
//   delta<0 — листать к более старой, delta>0 — к более новой (по кругу)
//   hold==true — стереть лог и выйти
//   click==true — закрыть без очистки
static inline bool error_ack_update(int8_t delta, bool click, bool hold, uint32_t /*now_ms*/) {
  if (!g_err_ack_active) return false;

  // выходы
  if (hold) {
    errlog_clear();           // очистить все записи
    g_err_ack_active = false; // и закрыть
    g_err_view_idx = -1;
    return false;
  }
  // ОТКЛЮЧЕНО: click больше не закрывает оверлей, только hold (долгое нажатие кнопки 2)
  // if (click) {
  //   g_err_ack_active = false; // закрыть без очистки
  //   return false;
  // }

  // актуальный count (вдруг что-то прилетело в рантайме)
  uint16_t n = errlog_count();
  if (n == 0) { // стало пусто — выходим
    g_err_ack_active = false;
    g_err_view_idx = -1;
    return false;
  }
  if (g_err_view_idx < 0) g_err_view_idx = 0;
  if ((uint16_t)g_err_view_idx >= n) g_err_view_idx = (int16_t)(n - 1);

  // прокрутка колесом энкодера (по кругу)
  if (delta != 0) {
    int32_t v = (int32_t)g_err_view_idx + (delta > 0 ? +1 : -1);
    // wrap
    while (v < 0)
      v += n;
    while (v >= (int32_t)n)
      v -= n;
    g_err_view_idx = (int16_t)v;
    draw_error_entry_4x16((uint16_t)g_err_view_idx);
  }
  return true;
}
