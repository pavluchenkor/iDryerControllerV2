#include "analog_buttons.h"

#include <Arduino.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_ADC_BTN
#define LOG_TAG "ADC BTN"
#include "debug_log.h"

#include "ADCbuttons.h"
#include "controller/control.h"
#include "error/error_eelog.h"
#include "hardware/hardware.h"
#include "menu/menu_state.h"
#include "service_screen/service_screen.h"
#include "service_screen/service_screen_state.h"

extern ADCbuttons adc_btns;

// Обработка аналоговых кнопок
// Кнопка 1 (ID 1, АДЦ≈20): клик → переключение юнита
// Кнопка 2 (ID 2, АДЦ≈3200): hold 1000ms → стоп текущего юнита
//                             hold 2000ms → стоп всех + очистка ошибок
void handleAnalogButtons(uint32_t now) {
  if (!hasScreen()) return;

  // Проверяем события кнопки 1
  ButtonEvent btn1_event = adc_btns.getEvent(1);
  switch (btn1_event) {
  case ButtonEvent::Click: {
    // Определяем показывается ли МЕНЮ (как в drawScreen logic)
    const uint8_t u = gscr_active_unit;
    const DryerController *ctrl = (u < NUM_UNITS) ? controllers[u] : nullptr;
    DryerMode mode = ctrl ? ctrl->mode() : DryerMode::Idle;
    ScreenState &ss = gscr_units[u];

    // Меню показывается когда: mode==Idle И было недавнее взаимодействие
    bool showingMenu = (mode == DryerMode::Idle) && ((now - ss.lastInputMs) <= IDLE_TO_STATUS_MS);

    if (showingMenu) {
      // МЕНЮ: переключаем только активный контроллер
      menu.number_controller = (menu.number_controller + 1) % menu.units_count;
      markUserInput(u, now); // Обновляем lastInputMs чтобы остаться в меню
    } else {
      // SERVICE SCREEN: переключаем юниты
      rotateToNextUnit(now);
    }
    break;
  }

  case ButtonEvent::Hold:
    DEBUG_INFO("Button 1 held for 1000ms (reserved)");
    break;

  default:
    break;
  }

  // Проверяем события кнопки 2
  ButtonEvent btn2_event = adc_btns.getEvent(2);
  switch (btn2_event) {
  case ButtonEvent::Click:
    DEBUG_I("Button 2 clicked (reserved)");
    break;

  case ButtonEvent::Hold:
    DEBUG_WARN("Button 2 held for 1000ms - stop current unit");
    if (controllers[gscr_active_unit]) {
      controllers[gscr_active_unit]->stop();
    }
    gscr_units[gscr_active_unit].kind = ScreenState::Kind::Menu;
    markUserInput(gscr_active_unit, now);
    break;

  case ButtonEvent::LongHold: {
    DEBUG_ERROR("Button 2 held for 2000ms - EMERGENCY STOP ALL");

    // Стоп всех юнитов
    uint8_t n = menu.units_count;
    if (n == 0 || n > NUM_UNITS) n = NUM_UNITS;
    for (uint8_t i = 0; i < n; ++i) {
      if (controllers[i]) controllers[i]->stop();
    }

    // Очистка ошибок и закрытие оверлея
    errlog_clear();
    g_err_ack_active = false;
    g_err_view_idx = -1;

    // Переход в меню
    gscr_units[gscr_active_unit].kind = ScreenState::Kind::Menu;
    markUserInput(gscr_active_unit, now);
    break;
  }

  default:
    break;
  }
}

