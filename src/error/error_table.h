#pragma once
#include <stddef.h>
#include "error_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

const char* errsev_name(ErrSeverity s);    // "INFO"
const char* errsrc_name(ErrSource s);      // "HEATER"
const char* errcode_name(ErrCode c);       // "HEATER_OVERMAX"
const char* errcode_human(ErrCode c);      // "Heater over maximum"

int error_format_line(const ErrorEvent* ev, char* out, size_t out_sz);
// "[ERR] ctrl=%u src=%s sev=%s code=%s msg=%s data=%ld t=%lu"

#ifdef __cplusplus
}
#endif