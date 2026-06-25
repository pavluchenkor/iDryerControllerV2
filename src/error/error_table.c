#include <stdio.h>
#include "error_table.h"
#include "error_defs.h"

/*** ERROR LOG FORMAT CONVENTION ***
* [severity][unit_id] 
* src:source_of_error
* code:error_code  
* msg:error_message
* data:data_at_the_time_of_the_error  
* t:time_of_the_error
************************************/


const char* errsev_name(ErrSeverity s){
  switch (s) {
#define X(en, str) case en: return str;
    ERRSEV_LIST(X)
#undef X
    default: return "SEV?";
  }
}

const char* errsrc_name(ErrSource s){
  switch (s) {
#define X(en, str) case en: return str;
    ERRSRC_LIST(X)
#undef X
    default: return "SRC?";
  }
}

const char* errcode_name(ErrCode c){
  switch (c) {
#define X(en, name, human) case en: return name;
    ERRCODE_LIST(X)
#undef X
    default: return "UNKNOWN_CODE";
  }
}

const char* errcode_human(ErrCode c){
  switch (c) {
#define X(en, name, human) case en: return human;
    ERRCODE_LIST(X)
#undef X
    default: return "Unknown code";
  }
}

int error_format_line(const ErrorEvent* ev, char* out, size_t out_sz){
  if (!out || !out_sz) return 0;
  const char* msg = (ev->msg && ev->msg[0]) ? ev->msg : errcode_name(ev->code);
  return snprintf(out, out_sz,
    // "[ERR][%u] src=%s sev=%s code=%s msg=%s data=%ld t=%lu",
    "  [%s][%u] src:%s code:%s  msg:%s  data:%ld  t:%lu",
    errsev_name(ev->severity),
    (unsigned)ev->ctrl_id,
    errsrc_name(ev->source),
    errcode_name(ev->code),
    msg,
    (long)ev->data,
    (unsigned long)ev->ts_ms
  );
}