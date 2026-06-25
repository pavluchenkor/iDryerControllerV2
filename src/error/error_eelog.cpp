// #pragma once
#include "error_eelog.h"
#include "error_defs.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_ERROR_PRINT
#define LOG_TAG "ERRLOG"
#include "debug_log.h"

/*** ERROR LOG FORMAT CONVENTION ***
 * [severity][unit_id]
 * src:source_of_error
 * code:error_code
 * msg:error_message
 * data:data_at_the_time_of_the_error
 * t:time_of_the_error
 ************************************/

// ----- макет EEPROM -----
// [HDR(12 байт)] [rec0(16)] [rec1(16)] ... [recN(16)]

struct __attribute__((packed)) ErrLogHdr {
  uint32_t magic; // 0xEDB055AA
  uint16_t ver;   // 1
  uint16_t cap;   // ERRLOG_CAP
  uint16_t head;  // next record index(0..cap-1)
  uint16_t count; // current quantity(0..cap)
};
static const uint32_t kMagic = 0xEDB055AA;
static const uint16_t kVer = 2; // bump: ErrLogRec._pad → count (дедупликация)

static inline size_t hdr_off() { return EE_ERR_OFF_HDR; }
static inline size_t rec_off(uint16_t idx) { return EE_ERR_OFF_RECS + (size_t)idx * ERRLOG_REC_SIZE; }
static size_t total_bytes() { return sizeof(ErrLogHdr) + (size_t)ERRLOG_CAP * sizeof(ErrLogRec); }

static ErrLogHdr hdr;

// --- низкоуровневые helpers ---
static void eep_read(size_t off, void *dst, size_t n) {
  for (size_t i = 0; i < n; i++)
    ((uint8_t *)dst)[i] = EEPROM.read(off + i);
}
static void eep_write(size_t off, const void *src, size_t n) {
  for (size_t i = 0; i < n; i++)
    EEPROM.write(off + i, ((const uint8_t *)src)[i]);
}

static void eep_write_guarded(size_t off, const void *src, size_t n) {
  const size_t LO = EE_ERR_OFF_HDR;                                         // 16
  const size_t HI = EE_ERR_OFF_RECS + (size_t)ERRLOG_CAP * ERRLOG_REC_SIZE; // 2076
  const size_t END = off + n;
  if (off < LO || END > HI) {
    DEBUG_C("EEPROM OOB write: off=%u n=%u (allowed %u..%u)", (unsigned)off, (unsigned)n, (unsigned)LO, (unsigned)HI);
    return; // ничего не пишем ловим ошибку
  }
  for (size_t i = 0; i < n; i++)
    EEPROM.write(off + i, ((const uint8_t *)src)[i]);
}


void errlog_init_eeprom(void) {
  // EEPROM.begin(total_bytes()); // уже должен быть вызван в main.cpp
  eep_read(hdr_off(), &hdr, sizeof(hdr));
  if (hdr.magic != kMagic || hdr.ver != kVer || hdr.cap != ERRLOG_CAP) {
    hdr.magic = kMagic;
    hdr.ver = kVer;
    hdr.cap = ERRLOG_CAP;
    hdr.head = 0;
    hdr.count = 0;
    // eep_write(hdr_off(), &hdr, sizeof(hdr));
    eep_write_guarded(hdr_off(), &hdr, sizeof(hdr));
    EEPROM.commit();
  }
}


void errlog_clear(void) {
  hdr.head = 0;
  hdr.count = 0;
  // eep_write(hdr_off(), &hdr, sizeof(hdr));
  eep_write_guarded(hdr_off(), &hdr, sizeof(hdr));
  EEPROM.commit();
}

void errlog_append_from_event(const ErrorEvent *ev) {
  if (!(ev->severity == ERRSEV_CRITICAL || ev->severity == ERRSEV_ERROR || ev->severity == ERRSEV_WARNING)) {
    return;
  }


  // Дедупликация: если та же ошибка уже есть — инкремент count, не новая запись.
  uint16_t oldest = (uint16_t)((hdr.head + hdr.cap - hdr.count) % hdr.cap);
  for (uint16_t i = 0; i < hdr.count; i++) {
    uint16_t idx = (uint16_t)((oldest + i) % hdr.cap);
    ErrLogRec existing{};
    eep_read(rec_off(idx), &existing, sizeof(existing));
    if (existing.code == (uint16_t)ev->code &&
        existing.source == (uint8_t)ev->source &&
        existing.ctrl_id == ev->ctrl_id) {
      if (existing.count < 255) existing.count++;
      existing.ts_ms = ev->ts_ms;
      eep_write_guarded(rec_off(idx), &existing, sizeof(existing));
      // Не вызываем EEPROM.commit() здесь: на RP2040 flash-запись блокирует
      // прерывания на ~50 мс и рушит I2C транзакцию. Commit произойдёт
      // при следующей новой записи или при вызове errlog_flush().
      return;
    }
  }

  ErrLogRec rec{};
  rec.ts_ms = ev->ts_ms;
  rec.data = ev->data;
  rec.code = (uint16_t)ev->code;
  rec.source = (uint8_t)ev->source;
  rec.severity = (uint8_t)ev->severity;
  rec.ctrl_id = ev->ctrl_id;
  rec.count = 1;

  // пишем в head
  // eep_write(rec_off(hdr.head), &rec, sizeof(rec));
  eep_write_guarded(rec_off(hdr.head), &rec, sizeof(rec));
  // сдвигаем head/count (кольцо)
  hdr.head = (uint16_t)((hdr.head + 1) % hdr.cap);
  if (hdr.count < hdr.cap) hdr.count++;
  // eep_write(hdr_off(), &hdr, sizeof(hdr));
  eep_write_guarded(hdr_off(), &hdr, sizeof(hdr));
  // if (rec.severity == ERRSEV_CRITICAL || rec.severity == ERRSEV_WARNING) {
  // EEPROM.commit(); EEPROM.commit();} // критичные ошибки сразу сохраняем
  EEPROM.commit();
}

uint16_t errlog_count(void) { return hdr.count; }

// i=0 -> самая старая; i=count-1 -> самая свежая
bool errlog_read_ith_oldest(uint16_t i, ErrLogRec *out) {
  if (i >= hdr.count) return false;
  // вычисляем индекс в кольце:
  // oldest = (head - count + cap) % cap
  uint16_t oldest = (uint16_t)((hdr.head + hdr.cap - hdr.count) % hdr.cap);
  uint16_t idx = (uint16_t)((oldest + i) % hdr.cap);
  eep_read(rec_off(idx), out, sizeof(*out));
  return true;
}

void errlog_dump_to_serial(void) {
  char line[160];
  for (uint16_t i = 0; i < hdr.count; ++i) {
    ErrLogRec r;
    if (!errlog_read_ith_oldest(i, &r)) break;

    // собираем временный ErrorEvent для красивого форматтера
    ErrorEvent ev;
    ev.ts_ms = r.ts_ms;
    ev.data = r.data;
    ev.code = (ErrCode)r.code;
    ev.source = (ErrSource)r.source;
    ev.severity = (ErrSeverity)r.severity;
    ev.ctrl_id = r.ctrl_id;
    ev.msg = errcode_human(ev.code); // строку в EEPROM не храним

    error_format_line(&ev, line, sizeof(line));
    switch (r.severity) {
    case ERRSEV_INFO:
      DEBUG_I("%s", line);
      break;
    case ERRSEV_WARNING:
      DEBUG_W("%s", line);
      break;
    case ERRSEV_ERROR:
      DEBUG_E("%s", line);
      break;
    case ERRSEV_CRITICAL:
      DEBUG_C("%s", line);
      break;
    default:
      break;
    }
  }
}


/** ---- SYS watchdog (0..15) ----  */
static uint8_t __sys_shadow[EE_SYS_AREA_SIZE];

void sys_shadow_init() {
  for (size_t i = 0; i < EE_SYS_AREA_SIZE; i++)
    __sys_shadow[i] = EEPROM.read(i);
}

void sys_shadow_check(const char *where) {
  uint8_t cur[EE_SYS_AREA_SIZE];
  for (size_t i = 0; i < EE_SYS_AREA_SIZE; i++)
    cur[i] = EEPROM.read(i);
  if (memcmp(cur, __sys_shadow, EE_SYS_AREA_SIZE) != 0) {
    uint32_t magic = *(uint32_t *)&cur[0];
    uint16_t ver = *(uint16_t *)&cur[4];
    uint32_t hours = *(uint32_t *)&cur[8];
    uint32_t minutes = *(uint32_t *)&cur[12];
    DEBUG_C("SYS CHANGED after %s  MAG=%08lx VER=%u  H=%lu  M=%lu", where, (unsigned long)magic, (unsigned)ver, (unsigned long)hours, (unsigned long)minutes);
    memcpy(__sys_shadow, cur, EE_SYS_AREA_SIZE);
  }
}