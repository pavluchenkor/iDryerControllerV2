#pragma once

#include <cstdint>
#include "ADCbuttons.h"  // для ButtonEvent enum

// Обработка аналоговых кнопок
// Доступна для всех режимов (с заглушкой для HARDWARE_MODE 2 и 3)
void handleAnalogButtons(uint32_t now);
