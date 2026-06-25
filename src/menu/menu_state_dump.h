#pragma once

#include "menu_bindings.h"
#include "menu_types.h"
#include "menu_eeprom.h"

#ifdef __cplusplus
class Print;                 // вперёд-объявление из Arduino
void print_all_settings(Print& out);
#endif