#pragma once
#include <stdint.h>

enum PortMode : uint8_t {
  PORT_EXT = 0,    // Дополнительный модуль сушилки (EXT)
  PORT_SCREEN = 1, // Экран OLED I2C
  PORT_SCALES = 2, // Весы HX711
  PORT_LINK = 3    // WiFi/Touch по UART
};

const char *portModeToString(PortMode mode);

// Валидация для меню (читает menu.port*_mode — желаемую конфигурацию)
bool isPortModeValid(uint8_t port, PortMode mode);

// Снапшот портов при старте. Вызвать один раз в setup() после loadFromEEPROM().
// Все runtime-запросы (hasScreen, hasLink, ...) работают по снапшоту.
void snapshotPortConfig();

// Вызвать после успешного обнаружения Link через кроватку (Port0).
// После этого hasLink() вернёт true даже если Port2/Port3 != PORT_LINK.
void setCradleLink(bool active);

// Runtime queries (читают снапшот, не menu.port*_mode)
bool hasScreen();          // Port3 == SCR
bool hasLink();            // любой порт == LNK
uint8_t getLinkPort();     // 2 или 3, 0 если нет
uint8_t getScalesPort();   // 1 или 2, 0 если нет
bool isPortExt(uint8_t port);
uint8_t calcMaxUnits();
