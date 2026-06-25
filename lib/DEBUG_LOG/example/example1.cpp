// heater.cpp
#define DEBUG_LOG 1               // включаем логи (или KASYAK_FINDER 1)
#define LOG_LEVEL LOG_LEVEL_DEBUG // уровень только для ЭТОГО файла
#include "debug_log.h"

void foo() {
  DEBUG_E("Overheat! t=%d\n", 125);
  DEBUG_D("pwm=%.1f%%\n", 37.5);
}
