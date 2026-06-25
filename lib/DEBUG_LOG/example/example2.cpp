// sensor.cpp
#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_WARN
// можно задать свой тег (иначе возьмётся "sensor.cpp")
#define LOG_TAG "SHT31"
#include "debug_log.h"

void bar() {
  DEBUG_W("CRC mismatch\n");
  DEBUG_I("ok\n");   // не выведется - уровень WARN
}
