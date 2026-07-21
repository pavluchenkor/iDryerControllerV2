#include "service_screen_state.h"

// если юнитов несколько - держим массив состояний
ScreenState gscr_units[NUM_UNITS];    // NUM_UNITS = 1..3 (из конфигурации)
uint8_t     gscr_active_unit = 0;     // какой юнит сейчас показываем
uint32_t    gscr_next_unit_rotate_ms = 0;
uint32_t    g_lastInputMs = 0;        // последний ввод (энкодер/кнопки), любой юнит

void markUserInput(uint8_t unit_id, uint32_t now) {
  auto &st = gscr_units[unit_id];
  st.lastInputMs = now;
  g_lastInputMs  = now;                             // глобальный таймер гашения экрана
  // Если были в пассивном IdleStatus - вернём меню.
  if (st.kind == ScreenState::Kind::IdleStatus) st.kind = ScreenState::Kind::Menu;
  st.nextSwapMs  = now + MODE_SWAP_MS;              // чтобы сразу вернуться на  режим 
  gscr_next_unit_rotate_ms = now + UNIT_ROTATE_MS;  // сбросить автоперелистыватель
}