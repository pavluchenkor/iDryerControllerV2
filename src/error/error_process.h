#pragma once
#include <stdint.h>
#include "error_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ErrorCallback)(const ErrorEvent* e);

void error_set_handler(ErrorCallback cb);
void error_set_min_severity(ErrSeverity s);
void error_enable_ctrl(uint8_t id);
void error_disable_ctrl(uint8_t id);

void error_process_all(void); // вытягивает все события и вызывает коллбэк

#ifdef __cplusplus
}
#endif