#pragma once

#include <stdint.h>

#include "menu/menu_state.h"

enum class SessionCounterKind : uint8_t {
  Drying = 0,
  Storage = 1,
  Profile = 2
};

// Восстанавливает/синхронизирует счётчики с backup-областью EEPROM.
// Возвращает true, если значения в menu были изменены.
bool sessionCountersRestoreFromBackup(MenuState &menu, bool layoutValid);

// Инкрементирует нужный счётчик и синхронно пишет:
// 1) основные поля в EEPROM (menu layout)
// 2) backup-запись в стабильной области EEPROM
// Возвращает новое значение инкрементированного счётчика.
uint16_t sessionCountersIncrement(MenuState &menu, SessionCounterKind kind);
