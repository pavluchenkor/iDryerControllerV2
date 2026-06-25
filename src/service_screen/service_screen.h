#pragma once
#include "UI/menu_ui.h"           // MenuUI
#include "controller/control.h"   // DryerMode
#include "sensor/Sensor.h"        // DryerInputs
#include "service_screen_state.h" // gscr_units[], константы
#include <U8g2lib.h>

#include "error/error_eelog.h"    // errlog_count()
#include "error/error_overlay.h"  // g_err_ack_active, g_err_view_idx, draw_error_entry_4x16
#include "service_screen_state.h" // gscr_next_unit_rotate_ms

#include "ADCbuttons.h"
#include "ISRbutton.h"
#include "ISRencoder.h"

// Экземпляр дисплея объявлен в main.cpp
// extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;
extern U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C u8g2;
extern DryerInputs readDryerInputs(uint8_t unit, uint32_t now);
extern DryerController *controllers[NUM_UNITS];
extern ISRencoder *enc;
extern ISRbutton *enc_btn;
extern ADCbuttons adc_btns;


// Рисовалки
void drawStatusError(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void drawStatusIdle(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void drawStatusDrying(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void drawStatusStorage(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void drawStatusProfile(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void drawStatusAutoTune(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void drawWeightScreen(const DryerInputs &in, uint8_t unit_id, const char *timeStr);
void rotateToNextUnit(uint32_t now);
void uiTick(uint32_t now, MenuUI &menuUi);
void errorOverlayTick(uint32_t now);

// Центральный диспетчер (ровно та сигнатура, что в main.cpp)
void drawScreen(uint32_t now, const DryerInputs &in, DryerMode mode, uint8_t unit_id, MenuUI &menuUi, const DryerController *ctrl);