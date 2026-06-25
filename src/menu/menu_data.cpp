// Auto-generated. Do not edit.
#include <stddef.h>
#include "menu_ids.h"
#include "menu_types.h"
#include "menu_state.h"

#ifdef __cplusplus
extern "C" {
#endif
void set_active_controller(void*);
void scales_count_set(void*);
void pid_recalc_heater(void*);
void pid_recalc_gain_heater(void*);
void pid_recalc_chamber(void*);
void pid_recalc_gain_chamber(void*);
void on_servo_preview(void*);
void ws_toggle(void*);
void units_count_set(void*);
void on_language_changed(void*);
void start_drying(void);
void start_storage(void);
void start_profile(void);
void start_drying_pla(void);
void start_drying_pla_cf(void);
void start_drying_pla_gf(void);
void start_drying_petg(void);
void start_drying_petg_cf(void);
void start_drying_petg_gf(void);
void start_drying_abs(void);
void start_drying_abs_cf(void);
void start_drying_abs_gf(void);
void start_drying_pa(void);
void start_drying_pa_cf(void);
void start_drying_pa_gf(void);
void start_drying_pc(void);
void start_drying_pc_cf(void);
void start_drying_my1(void);
void start_drying_my2(void);
void start_drying_my3(void);
void calib_zero1(void);
void calib_kg_1(void);
void calib_zero2(void);
void calib_kg_2(void);
void calib_zero3(void);
void calib_kg_3(void);
void calib_zero4(void);
void calib_kg_4(void);
void pid_autotune_heater(void);
void pid_autotune_chamber(void);
void start_claim(void);
#ifdef __cplusplus
}
#endif

extern MenuState menu;

const MenuItem g_menu[MENU__COUNT] = {
  [0] = {
    MENU_ROOT, { "МЕНЮ", "MENU" }, { nullptr, nullptr },
    MN_SUBMENU, -1, 1, 8,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [1] = {
    MENU_CONTROLLER_CHOICE, { "КОНТРОЛЛЕР", "CONTROLLER" }, { nullptr, nullptr },
    MN_VALUE, 0, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.number_controller, 0, 2, 1, set_active_controller, false } },
    2220, 1
  },
  [2] = {
    MENU_DRYING, { "СУШКА", "DRYING" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 3, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [3] = {
    MENU_DRY_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 2, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.dry_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [4] = {
    MENU_DRY_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 2, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.dry_time, 0, 600, 1, nullptr, false } },
    -1, 0
  },
  [5] = {
    MENU_DRY_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 2, -1, 0,
    { { start_drying }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [6] = {
    MENU_STORAGE, { "ХРАНЕНИЕ", "STORAGE" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 7, 4,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [7] = {
    MENU_STORAGE_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 6, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.storage_temp, 35, 90, 1, nullptr, false } },
    -1, 0
  },
  [8] = {
    MENU_STORAGE_HUM, { "ВЛАЖНОСТЬ", "HUMIDITY" }, { "%RH", "%RH" },
    MN_VALUE, 6, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.storage_hum, 5, 30, 1, nullptr, false } },
    -1, 0
  },
  [9] = {
    MENU_STORAGE_HUM_PRIORITY, { "ПО ВЛАЖНОСТИ", "BY HUMIDITY" }, { nullptr, nullptr },
    MN_TOGGLE, 6, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.storage_hum_priority, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [10] = {
    MENU_STORAGE_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 6, -1, 0,
    { { start_storage }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [11] = {
    MENU_PROFILE, { "ПРОФИЛЬ", "PROFILE" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 12, 11,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [12] = {
    MENU_PROFILE_START, { "СТАРТ ПРОФИЛЯ", "PROFILE START" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 13, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [13] = {
    MENU_START_STAGE_BY_NUMBER, { "ПЕРВЫЙ ЭТАП", "FIRST STAGE" }, { "", "" },
    MN_VALUE, 12, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.start_stage_by_number, 1, 10, 1, nullptr, false } },
    -1, 0
  },
  [14] = {
    MENU_START_STAGE, { "СТАРТ ПРОФИЛЯ", "PROFILE START" }, { nullptr, nullptr },
    MN_ACTION, 12, -1, 0,
    { { start_profile }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [15] = {
    MENU_STAGE_01, { "ЭТАП 01", "STAGE 01" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 16, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [16] = {
    MENU_STAGE_01_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 15, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_01_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [17] = {
    MENU_STAGE_01_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 15, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_01_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [18] = {
    MENU_STAGE_01_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 15, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_01_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [19] = {
    MENU_STAGE_02, { "ЭТАП 02", "STAGE 02" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 20, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [20] = {
    MENU_STAGE_02_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 19, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_02_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [21] = {
    MENU_STAGE_02_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 19, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_02_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [22] = {
    MENU_STAGE_02_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 19, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_02_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [23] = {
    MENU_STAGE_03, { "ЭТАП 03", "STAGE 03" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 24, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [24] = {
    MENU_STAGE_03_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 23, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_03_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [25] = {
    MENU_STAGE_03_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 23, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_03_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [26] = {
    MENU_STAGE_03_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 23, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_03_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [27] = {
    MENU_STAGE_04, { "ЭТАП 04", "STAGE 04" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 28, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [28] = {
    MENU_STAGE_04_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 27, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_04_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [29] = {
    MENU_STAGE_04_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 27, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_04_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [30] = {
    MENU_STAGE_04_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 27, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_04_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [31] = {
    MENU_STAGE_05, { "ЭТАП 05", "STAGE 05" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 32, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [32] = {
    MENU_STAGE_05_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 31, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_05_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [33] = {
    MENU_STAGE_05_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 31, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_05_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [34] = {
    MENU_STAGE_05_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 31, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_05_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [35] = {
    MENU_STAGE_06, { "ЭТАП 06", "STAGE 06" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 36, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [36] = {
    MENU_STAGE_06_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 35, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_06_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [37] = {
    MENU_STAGE_06_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 35, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_06_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [38] = {
    MENU_STAGE_06_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 35, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_06_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [39] = {
    MENU_STAGE_07, { "ЭТАП 07", "STAGE 07" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 40, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [40] = {
    MENU_STAGE_07_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 39, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_07_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [41] = {
    MENU_STAGE_07_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 39, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_07_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [42] = {
    MENU_STAGE_07_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 39, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_07_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [43] = {
    MENU_STAGE_08, { "ЭТАП 08", "STAGE 08" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 44, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [44] = {
    MENU_STAGE_08_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 43, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_08_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [45] = {
    MENU_STAGE_08_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 43, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_08_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [46] = {
    MENU_STAGE_08_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 43, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_08_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [47] = {
    MENU_STAGE_09, { "ЭТАП 09", "STAGE 09" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 48, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [48] = {
    MENU_STAGE_09_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 47, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_09_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [49] = {
    MENU_STAGE_09_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 47, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_09_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [50] = {
    MENU_STAGE_09_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 47, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_09_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [51] = {
    MENU_STAGE_10, { "ЭТАП 10", "STAGE 10" }, { nullptr, nullptr },
    MN_SUBMENU, 11, 52, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [52] = {
    MENU_STAGE_10_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 51, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.profile_stage_10_temp, 30, 110, 1, nullptr, false } },
    -1, 0
  },
  [53] = {
    MENU_STAGE_10_RAMPS, { "ПОДЪЕМ", "RAMP" }, { "мин", "min" },
    MN_VALUE, 51, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_10_ramps_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [54] = {
    MENU_STAGE_10_HOLD, { "УДЕРЖАНИЕ", "HOLD" }, { "мин", "min" },
    MN_VALUE, 51, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_stage_10_time, 0, 1200, 1, nullptr, false } },
    -1, 0
  },
  [55] = {
    MENU_PRESETS, { "ПРЕСЕТЫ", "PRESETS" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 56, 17,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [56] = {
    MENU_PRESET_PLA, { "PLA", "PLA" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 57, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [57] = {
    MENU_PRESET_PLA_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 56, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pla_temp, 35, 55, 1, nullptr, false } },
    2401, 4
  },
  [58] = {
    MENU_PRESET_PLA_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 56, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pla_time, 0, 600, 5, nullptr, false } },
    2405, 2
  },
  [59] = {
    MENU_PRESET_PLA_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 56, -1, 0,
    { { start_drying_pla }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [60] = {
    MENU_PRESET_PLA_CF, { "PLA-CF", "PLA-CF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 61, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [61] = {
    MENU_PRESET_PLA_CF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 60, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pla_cf_temp, 35, 55, 1, nullptr, false } },
    2407, 4
  },
  [62] = {
    MENU_PRESET_PLA_CF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 60, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pla_cf_time, 0, 600, 5, nullptr, false } },
    2411, 2
  },
  [63] = {
    MENU_PRESET_PLA_CF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 60, -1, 0,
    { { start_drying_pla_cf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [64] = {
    MENU_PRESET_PLA_GF, { "PLA-GF", "PLA-GF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 65, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [65] = {
    MENU_PRESET_PLA_GF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 64, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pla_gf_temp, 35, 55, 1, nullptr, false } },
    2413, 4
  },
  [66] = {
    MENU_PRESET_PLA_GF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 64, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pla_gf_time, 0, 600, 5, nullptr, false } },
    2417, 2
  },
  [67] = {
    MENU_PRESET_PLA_GF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 64, -1, 0,
    { { start_drying_pla_gf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [68] = {
    MENU_PRESET_PETG, { "PETG", "PETG" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 69, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [69] = {
    MENU_PRESET_PETG_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 68, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_petg_temp, 50, 70, 1, nullptr, false } },
    2419, 4
  },
  [70] = {
    MENU_PRESET_PETG_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 68, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_petg_time, 0, 600, 5, nullptr, false } },
    2423, 2
  },
  [71] = {
    MENU_PRESET_PETG_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 68, -1, 0,
    { { start_drying_petg }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [72] = {
    MENU_PRESET_PETG_CF, { "PETG-CF", "PETG-CF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 73, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [73] = {
    MENU_PRESET_PETG_CF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 72, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_petg_cf_temp, 55, 75, 1, nullptr, false } },
    2425, 4
  },
  [74] = {
    MENU_PRESET_PETG_CF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 72, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_petg_cf_time, 0, 600, 5, nullptr, false } },
    2429, 2
  },
  [75] = {
    MENU_PRESET_PETG_CF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 72, -1, 0,
    { { start_drying_petg_cf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [76] = {
    MENU_PRESET_PETG_GF, { "PETG-GF", "PETG-GF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 77, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [77] = {
    MENU_PRESET_PETG_GF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 76, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_petg_gf_temp, 50, 70, 1, nullptr, false } },
    2431, 4
  },
  [78] = {
    MENU_PRESET_PETG_GF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 76, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_petg_gf_time, 0, 600, 5, nullptr, false } },
    2435, 2
  },
  [79] = {
    MENU_PRESET_PETG_GF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 76, -1, 0,
    { { start_drying_petg_gf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [80] = {
    MENU_PRESET_ABS, { "ABS", "ABS" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 81, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [81] = {
    MENU_PRESET_ABS_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 80, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_abs_temp, 70, 90, 1, nullptr, false } },
    2437, 4
  },
  [82] = {
    MENU_PRESET_ABS_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 80, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_abs_time, 0, 600, 5, nullptr, false } },
    2441, 2
  },
  [83] = {
    MENU_PRESET_ABS_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 80, -1, 0,
    { { start_drying_abs }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [84] = {
    MENU_PRESET_ABS_CF, { "ABS-CF", "ABS-CF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 85, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [85] = {
    MENU_PRESET_ABS_CF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 84, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_abs_cf_temp, 80, 100, 1, nullptr, false } },
    2443, 4
  },
  [86] = {
    MENU_PRESET_ABS_CF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 84, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_abs_cf_time, 0, 600, 5, nullptr, false } },
    2447, 2
  },
  [87] = {
    MENU_PRESET_ABS_CF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 84, -1, 0,
    { { start_drying_abs_cf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [88] = {
    MENU_PRESET_ABS_GF, { "ABS-GF", "ABS-GF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 89, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [89] = {
    MENU_PRESET_ABS_GF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 88, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_abs_gf_temp, 80, 100, 1, nullptr, false } },
    2449, 4
  },
  [90] = {
    MENU_PRESET_ABS_GF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 88, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_abs_gf_time, 0, 600, 5, nullptr, false } },
    2453, 2
  },
  [91] = {
    MENU_PRESET_ABS_GF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 88, -1, 0,
    { { start_drying_abs_gf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [92] = {
    MENU_PRESET_PA, { "PA", "PA" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 93, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [93] = {
    MENU_PRESET_PA_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 92, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pa_temp, 80, 100, 1, nullptr, false } },
    2455, 4
  },
  [94] = {
    MENU_PRESET_PA_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 92, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pa_time, 0, 600, 5, nullptr, false } },
    2459, 2
  },
  [95] = {
    MENU_PRESET_PA_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 92, -1, 0,
    { { start_drying_pa }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [96] = {
    MENU_PRESET_PA_CF, { "PA-CF", "PA-CF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 97, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [97] = {
    MENU_PRESET_PA_CF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 96, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pa_cf_temp, 90, 110, 1, nullptr, false } },
    2461, 4
  },
  [98] = {
    MENU_PRESET_PA_CF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 96, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pa_cf_time, 0, 600, 5, nullptr, false } },
    2465, 2
  },
  [99] = {
    MENU_PRESET_PA_CF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 96, -1, 0,
    { { start_drying_pa_cf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [100] = {
    MENU_PRESET_PA_GF, { "PA-GF", "PA-GF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 101, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [101] = {
    MENU_PRESET_PA_GF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 100, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pa_gf_temp, 90, 110, 1, nullptr, false } },
    2467, 4
  },
  [102] = {
    MENU_PRESET_PA_GF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 100, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pa_gf_time, 0, 600, 5, nullptr, false } },
    2471, 2
  },
  [103] = {
    MENU_PRESET_PA_GF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 100, -1, 0,
    { { start_drying_pa_gf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [104] = {
    MENU_PRESET_PC, { "PC", "PC" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 105, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [105] = {
    MENU_PRESET_PC_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 104, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pc_temp, 90, 110, 1, nullptr, false } },
    2473, 4
  },
  [106] = {
    MENU_PRESET_PC_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 104, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pc_time, 0, 600, 5, nullptr, false } },
    2477, 2
  },
  [107] = {
    MENU_PRESET_PC_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 104, -1, 0,
    { { start_drying_pc }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [108] = {
    MENU_PRESET_PC_CF, { "PC-CF", "PC-CF" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 109, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [109] = {
    MENU_PRESET_PC_CF_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 108, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_pc_cf_temp, 90, 110, 1, nullptr, false } },
    2479, 4
  },
  [110] = {
    MENU_PRESET_PC_CF_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 108, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_pc_cf_time, 0, 600, 5, nullptr, false } },
    2483, 2
  },
  [111] = {
    MENU_PRESET_PC_CF_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 108, -1, 0,
    { { start_drying_pc_cf }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [112] = {
    MENU_PRESET_MY1, { "MY1", "MY1" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 113, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [113] = {
    MENU_PRESET_MY1_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 112, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_my1_temp, 60, 80, 1, nullptr, false } },
    2485, 4
  },
  [114] = {
    MENU_PRESET_MY1_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 112, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_my1_time, 0, 600, 5, nullptr, false } },
    2489, 2
  },
  [115] = {
    MENU_PRESET_MY1_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 112, -1, 0,
    { { start_drying_my1 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [116] = {
    MENU_PRESET_MY2, { "MY2", "MY2" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 117, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [117] = {
    MENU_PRESET_MY2_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 116, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_my2_temp, 70, 90, 1, nullptr, false } },
    2491, 4
  },
  [118] = {
    MENU_PRESET_MY2_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 116, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_my2_time, 0, 600, 5, nullptr, false } },
    2495, 2
  },
  [119] = {
    MENU_PRESET_MY2_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 116, -1, 0,
    { { start_drying_my2 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [120] = {
    MENU_PRESET_MY3, { "MY3", "MY3" }, { nullptr, nullptr },
    MN_SUBMENU, 55, 121, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [121] = {
    MENU_PRESET_MY3_TEMP, { "ТЕМПЕРАТУРА", "TEMPERATURE" }, { "°C", "°C" },
    MN_VALUE, 120, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.preset_my3_temp, 80, 100, 1, nullptr, false } },
    2497, 4
  },
  [122] = {
    MENU_PRESET_MY3_TIME, { "ВРЕМЯ", "TIME" }, { "мин", "min" },
    MN_VALUE, 120, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.preset_my3_time, 0, 600, 5, nullptr, false } },
    2501, 2
  },
  [123] = {
    MENU_PRESET_MY3_START, { "СТАРТ", "START" }, { nullptr, nullptr },
    MN_ACTION, 120, -1, 0,
    { { start_drying_my3 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [124] = {
    MENU_SCALES, { "ВЕСЫ", "SCALES" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 125, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [125] = {
    MENU_SCALES_COUNT, { "КОЛ-ВО МОДУЛЕЙ", "NUMBER OF MODULES" }, { nullptr, nullptr },
    MN_VALUE, 124, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.scales_count, 0, 4, 1, scales_count_set, false } },
    2503, 1
  },
  [126] = {
    MENU_TARE, { "ТАРА", "TARE" }, { nullptr, nullptr },
    MN_SUBMENU, 124, 127, 4,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [127] = {
    MENU_TARE_SPOOL1, { "КАТУШКА 1", "SPOOL 1" }, { "г", "g" },
    MN_VALUE, 126, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.tare_spool1, 0, 2000, 1, nullptr, false } },
    2504, 4
  },
  [128] = {
    MENU_TARE_SPOOL2, { "КАТУШКА 2", "SPOOL 2" }, { "г", "g" },
    MN_VALUE, 126, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.tare_spool2, 0, 2000, 1, nullptr, false } },
    2508, 4
  },
  [129] = {
    MENU_TARE_SPOOL3, { "КАТУШКА 3", "SPOOL 3" }, { "г", "g" },
    MN_VALUE, 126, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.tare_spool3, 0, 2000, 1, nullptr, false } },
    2512, 4
  },
  [130] = {
    MENU_TARE_SPOOL4, { "КАТУШКА 4", "SPOOL 4" }, { "г", "g" },
    MN_VALUE, 126, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.tare_spool4, 0, 2000, 1, nullptr, false } },
    2516, 4
  },
  [131] = {
    MENU_CALIBRATION, { "КАЛИБРОВКА", "CALIBRATION" }, { nullptr, nullptr },
    MN_SUBMENU, 124, 132, 4,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [132] = {
    MENU_CALIB_SPOOL1, { "КАТУШКА 1", "SPOOL 1" }, { nullptr, nullptr },
    MN_SUBMENU, 131, 133, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [133] = {
    MENU_CALIB_ZERO1, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
    MN_ACTION, 132, -1, 0,
    { { calib_zero1 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [134] = {
    MENU_CALIB_KG_1, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
    MN_ACTION, 132, -1, 0,
    { { calib_kg_1 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [135] = {
    MENU_CALIB_SPOOL2, { "КАТУШКА 2", "SPOOL 2" }, { nullptr, nullptr },
    MN_SUBMENU, 131, 136, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [136] = {
    MENU_CALIB_ZERO2, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
    MN_ACTION, 135, -1, 0,
    { { calib_zero2 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [137] = {
    MENU_CALIB_KG_2, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
    MN_ACTION, 135, -1, 0,
    { { calib_kg_2 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [138] = {
    MENU_CALIB_SPOOL3, { "КАТУШКА 3", "SPOOL 3" }, { nullptr, nullptr },
    MN_SUBMENU, 131, 139, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [139] = {
    MENU_CALIB_ZERO3, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
    MN_ACTION, 138, -1, 0,
    { { calib_zero3 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [140] = {
    MENU_CALIB_KG_3, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
    MN_ACTION, 138, -1, 0,
    { { calib_kg_3 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [141] = {
    MENU_CALIB_SPOOL4, { "КАТУШКА 4", "SPOOL 4" }, { nullptr, nullptr },
    MN_SUBMENU, 131, 142, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [142] = {
    MENU_CALIB_ZERO4, { "ПРИНЯТЬ НОЛЬ", "SET ZERO" }, { nullptr, nullptr },
    MN_ACTION, 141, -1, 0,
    { { calib_zero4 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [143] = {
    MENU_CALIB_KG_4, { "ПРИНЯТЬ 1000 Г", "SET 1000 G" }, { nullptr, nullptr },
    MN_ACTION, 141, -1, 0,
    { { calib_kg_4 }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [144] = {
    MENU_SETTINGS, { "НАСТРОЙКИ", "SETTINGS" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 145, 6,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [145] = {
    MENU_STORAGE_LOGIC, { "ХРАНЕНИЕ", "STORAGE" }, { nullptr, nullptr },
    MN_SUBMENU, 144, 146, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [146] = {
    MENU_STORAGE_AUTO, { "АВТО ХРАНЕНИЕ", "AUTO STORAGE" }, { nullptr, nullptr },
    MN_TOGGLE, 145, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.storage_auto, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [147] = {
    MENU_STORAGE_AUTO_DRY, { "АВТО СУШКА", "AUTO DRY" }, { nullptr, nullptr },
    MN_TOGGLE, 145, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.storage_auto_dry, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [148] = {
    MENU_STORAGE_RH, { "ПО ВЛАЖНОСТИ", "BY HUMID" }, { nullptr, nullptr },
    MN_SUBMENU, 145, 149, 1,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [149] = {
    MENU_STORAGE_RH_HYST, { "ГИСТЕРЕЗИС %", "HYSTERESIS %" }, { "%", "%" },
    MN_VALUE, 148, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.storage_rh_hyst, 1, 10, 1, nullptr, false } },
    -1, 0
  },
  [150] = {
    MENU_STORAGE_TEMP_SETUP, { "ПО ТЕМПЕРАТУРЕ", "BY TEMP" }, { nullptr, nullptr },
    MN_SUBMENU, 145, 151, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [151] = {
    MENU_STORAGE_DRY_TEMP_MODE, { "АБС/%СУШКИ", "ABS/%DRY" }, { nullptr, nullptr },
    MN_TOGGLE, 150, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.storage_dry_temp_mode, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [152] = {
    MENU_STORAGE_DRY_TEMP_BY_DRY_TEMP, { "% ОТ T СУШКИ", "% OF DRY TEMP" }, { "%", "%" },
    MN_VALUE, 150, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.storage_dry_temp_by_dry_temp, 50, 100, 1, nullptr, false } },
    -1, 0
  },
  [153] = {
    MENU_STORAGE_COMMON, { "ОБЩЕЕ", "COMMON" }, { nullptr, nullptr },
    MN_SUBMENU, 145, 154, 1,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [154] = {
    MENU_STORAGE_MIN_HOLD_SEC, { "МИН. СОСТОЯНИЕ", "MIN HOLD" }, { "с", "s" },
    MN_VALUE, 153, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.storage_min_hold_sec, 5, 600, 5, nullptr, false } },
    -1, 0
  },
  [155] = {
    MENU_PID_HEATER, { "ПИД НАГРЕВАТЕЛЬ", "PID HEATER" }, { nullptr, nullptr },
    MN_SUBMENU, 144, 156, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [156] = {
    MENU_PID_KP_HEATER, { "КП", "Kp" }, { nullptr, nullptr },
    MN_VALUE, 155, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_kp_heater, 0.1, 1000, 0.1, pid_recalc_heater, false } },
    -1, 0
  },
  [157] = {
    MENU_PID_KI_HEATER, { "КИ", "Ki" }, { nullptr, nullptr },
    MN_VALUE, 155, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_ki_heater, 0, 1000, 0.01, pid_recalc_heater, false } },
    -1, 0
  },
  [158] = {
    MENU_PID_KD_HEATER, { "КД", "Kd" }, { nullptr, nullptr },
    MN_VALUE, 155, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_kd_heater, 0, 1000, 0.1, pid_recalc_heater, false } },
    -1, 0
  },
  [159] = {
    MENU_PID_GAIN_HEATER, { "GAIN", "GAIN" }, { "×", "×" },
    MN_VALUE, 155, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_gain_heater, 0.1, 50, 0.01, pid_recalc_gain_heater, false } },
    -1, 0
  },
  [160] = {
    MENU_PID_AUTOTUNE_HEATER, { "Автопид", "Autopid" }, { nullptr, nullptr },
    MN_ACTION, 155, -1, 0,
    { { pid_autotune_heater }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [161] = {
    MENU_PID_CHAMBER, { "ПИД ВОЗДУХ", "PID CHAMBER" }, { nullptr, nullptr },
    MN_SUBMENU, 144, 162, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [162] = {
    MENU_PID_KP_CHAMBER, { "КП", "Kp" }, { nullptr, nullptr },
    MN_VALUE, 161, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_kp_chamber, 0.1, 1000, 0.1, pid_recalc_chamber, false } },
    -1, 0
  },
  [163] = {
    MENU_PID_KI_CHAMBER, { "КИ", "Ki" }, { nullptr, nullptr },
    MN_VALUE, 161, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_ki_chamber, 0, 1000, 0.01, pid_recalc_chamber, false } },
    -1, 0
  },
  [164] = {
    MENU_PID_KD_CHAMBER, { "КД", "Kd" }, { nullptr, nullptr },
    MN_VALUE, 161, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_kd_chamber, 0, 1000, 0.1, pid_recalc_chamber, false } },
    -1, 0
  },
  [165] = {
    MENU_PID_GAIN_CHAMBER, { "GAIN", "GAIN" }, { "×", "×" },
    MN_VALUE, 161, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.pid_gain_chamber, 0.1, 200, 0.01, pid_recalc_gain_chamber, false } },
    -1, 0
  },
  [166] = {
    MENU_PID_AUTOTUNE_CHAMBER, { "Автопид", "Autopid" }, { nullptr, nullptr },
    MN_ACTION, 161, -1, 0,
    { { pid_autotune_chamber }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [167] = {
    MENU_HEATING, { "НАГРЕВ", "HEATING" }, { nullptr, nullptr },
    MN_SUBMENU, 144, 168, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [168] = {
    MENU_HEATER_MAX_TEMP, { "МАКС.НАГРЕВАТЕЛЬ", "MAX TEMP" }, { "°C", "°C" },
    MN_VALUE, 167, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.heater_max_temp, 30, 150, 1, nullptr, false } },
    -1, 0
  },
  [169] = {
    MENU_AIR_MAX_TEMP, { "МАКС.ТЕМП.ВОЗДУХ", "MAX AIR TEMP" }, { "°C", "°C" },
    MN_VALUE, 167, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.air_max_temp, 30, 120, 1, nullptr, false } },
    -1, 0
  },
  [170] = {
    MENU_DELTA, { "ДЕЛЬТА", "DELTA" }, { "°C", "°C" },
    MN_VALUE, 167, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.delta_c, 0, 55, 1, nullptr, false } },
    -1, 0
  },
  [171] = {
    MENU_HEATER_DELTA_IS_PERCENT, { "ДЕЛЬТА АБС/%", "DELTA ABS/%" }, { nullptr, nullptr },
    MN_TOGGLE, 167, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.heater_delta_is_percent, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [172] = {
    MENU_HEATING_CHECK, { "ПРОВЕРКА", "CHECK" }, { nullptr, nullptr },
    MN_SUBMENU, 167, 173, 4,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [173] = {
    MENU_VH_MAX_ERR, { "ПОРОГ ОШИБКИ", "ERROR LIMIT" }, { nullptr, nullptr },
    MN_VALUE, 172, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.vh_max_err, 0, 500, 10, nullptr, false } },
    -1, 0
  },
  [174] = {
    MENU_VH_HEATING_GAIN, { "ПРИРОСТ T", "TEMP GAIN" }, { "°C", "°C" },
    MN_VALUE, 172, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.vh_heating_gain, 0.1, 10, 0.1, nullptr, false } },
    -1, 0
  },
  [175] = {
    MENU_VH_CHECK_GAIN_TIME, { "ОКНО ПРОВЕРКИ", "CHECK WINDOW" }, { "с", "s" },
    MN_VALUE, 172, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.vh_check_gain_time_s, 5, 120, 1, nullptr, false } },
    -1, 0
  },
  [176] = {
    MENU_VH_ARM_PWM_MIN, { "ШИМ СТАРТ %", "PWM START %" }, { "%", "%" },
    MN_VALUE, 172, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.vh_arm_pwm_min_pct, 10, 100, 5, nullptr, false } },
    -1, 0
  },
  [177] = {
    MENU_FAN, { "ВЕНТИЛЯТОР", "FAN" }, { nullptr, nullptr },
    MN_SUBMENU, 144, 178, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [178] = {
    MENU_FAN_TEMP_ON, { "ПОРОГ T ВКЛ", "ON THRESHOLD" }, { "°C", "°C" },
    MN_VALUE, 177, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.fan_temp_on_c, 40, 70, 0.5, nullptr, false } },
    -1, 0
  },
  [179] = {
    MENU_FAN_HYST, { "ГИСТЕРЕЗИС", "HYSTERESIS" }, { "°C", "°C" },
    MN_VALUE, 177, -1, 0,
    { { NULL }, { VT_F32, (void*)&menu.fan_hyst_c, 5, 20, 0.5, nullptr, false } },
    -1, 0
  },
  [180] = {
    MENU_SERVO, { "СЕРВО", "SERVO" }, { nullptr, nullptr },
    MN_SUBMENU, 144, 181, 5,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [181] = {
    MENU_SERVO_CLOSED_ANGLE, { "ЗАКРЫТО УГОЛ", "CLOSED ANGLE" }, { "°", "°" },
    MN_VALUE, 180, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.servo_closed_angle, 0, 180, 1, on_servo_preview, false } },
    -1, 0
  },
  [182] = {
    MENU_SERVO_OPEN_ANGLE, { "ОТКРЫТО УГОЛ", "OPEN ANGLE" }, { "°", "°" },
    MN_VALUE, 180, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.servo_open_angle, 0, 180, 1, on_servo_preview, false } },
    -1, 0
  },
  [183] = {
    MENU_SERVO_TIME_CLOSED, { "ВРЕМЯ ЗАКРЫТО", "CLOSED TIME" }, { "с", "s" },
    MN_VALUE, 180, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.servo_time_closed, 0, 3600, 1, nullptr, false } },
    -1, 0
  },
  [184] = {
    MENU_SERVO_TIME_OPEN, { "ВРЕМЯ ОТКРЫТО", "OPEN TIME" }, { "с", "s" },
    MN_VALUE, 180, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.servo_time_open, 0, 600, 1, nullptr, false } },
    -1, 0
  },
  [185] = {
    MENU_SERVO_SMART_MODE, { "УМНЫЙ РЕЖИМ", "SMART MODE" }, { nullptr, nullptr },
    MN_TOGGLE, 180, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.servo_smart_mode, 0, 0, 1, nullptr, false } },
    -1, 0
  },
  [186] = {
    MENU_GLOBAL_SETTINGS, { "ОБЩИЕ", "GLOBAL" }, { nullptr, nullptr },
    MN_SUBMENU, 0, 187, 6,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [187] = {
    MENU_PIORTAL, { "ПОРТАЛ", "PORTAL" }, { nullptr, nullptr },
    MN_SUBMENU, 186, 188, 2,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [188] = {
    MENU_START_CLAIM, { "СВЯЗАТЬ", "CLAIM" }, { nullptr, nullptr },
    MN_ACTION, 187, -1, 0,
    { { start_claim }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [189] = {
    MENU_IGNORE_EXTERNAL_CMD, { "ИГНОР КОМАНД", "IGNOR EXT CMD" }, { nullptr, nullptr },
    MN_TOGGLE, 187, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.ign_ext_cmd, 0, 0, 1, nullptr, false } },
    2745, 1
  },
  [190] = {
    MENU_SESSION_COUNT, { "СЧЕТЧИК СЕССИЙ", "SESSION COUNTER" }, { nullptr, nullptr },
    MN_SUBMENU, 186, 191, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [191] = {
    MENU_DRYING_SESSION_COUNT, { "СУШКА", "DRYING" }, { nullptr, nullptr },
    MN_VALUE, 190, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.drying_session_count, 0, 65535, 1, nullptr, false } },
    2746, 2
  },
  [192] = {
    MENU_STORAGE_SESSION_COUNT, { "ХРАНЕНИЕ", "STORAGE" }, { nullptr, nullptr },
    MN_VALUE, 190, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.storage_session_count, 0, 65535, 1, nullptr, false } },
    2748, 2
  },
  [193] = {
    MENU_PROFILE_SESSION_COUNT, { "ПРОФИЛЬ", "PROFILE" }, { nullptr, nullptr },
    MN_VALUE, 190, -1, 0,
    { { NULL }, { VT_U16, (void*)&menu.profile_session_count, 0, 65535, 1, nullptr, false } },
    2750, 2
  },
  [194] = {
    MENU_HARDWARE_CONFIG, { "КОНФИГУРАЦИЯ ПОРТОВ", "PORT CONFIG" }, { nullptr, nullptr },
    MN_SUBMENU, 186, 195, 3,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [195] = {
    MENU_PORT1_MODE, { "ПОРТ 1", "PORT 1" }, { nullptr, nullptr },
    MN_VALUE, 194, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.port1_mode, 0, 3, 1, nullptr, false } },
    2752, 1
  },
  [196] = {
    MENU_PORT2_MODE, { "ПОРТ 2", "PORT 2" }, { nullptr, nullptr },
    MN_VALUE, 194, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.port2_mode, 0, 3, 1, nullptr, false } },
    2753, 1
  },
  [197] = {
    MENU_PORT3_MODE, { "ПОРТ 3", "PORT 3" }, { nullptr, nullptr },
    MN_VALUE, 194, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.port3_mode, 0, 3, 1, nullptr, false } },
    2754, 1
  },
  [198] = {
    MENU_WS_LOCAL, { "WS ЛОКАЛЬНЫЙ", "WS LOCAL" }, { nullptr, nullptr },
    MN_SUBMENU, 186, 199, 1,
    { { NULL }, { VT_F32, NULL, 0, 0, 0, NULL, false } },
    -1, 0
  },
  [199] = {
    MENU_WS_ENABLED, { "ВКЛ/ВЫКЛ", "ON/OFF" }, { nullptr, nullptr },
    MN_TOGGLE, 198, -1, 0,
    { { NULL }, { VT_BOOL, (void*)&menu.ws_enabled, 0, 0, 1, ws_toggle, false } },
    2755, 1
  },
  [200] = {
    MENU_UNITS_COUNT, { "КОЛ-ВО ЮНИТОВ", "UNITS" }, { nullptr, nullptr },
    MN_VALUE, 186, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.units_count, 1, 3, 1, units_count_set, false } },
    2756, 1
  },
  [201] = {
    MENU_LANGUAGE, { "ЯЗЫК", "LANGUAGE" }, { nullptr, nullptr },
    MN_VALUE, 186, -1, 0,
    { { NULL }, { VT_U8, (void*)&menu.language, 0, 1, 1, on_language_changed, true } },
    2757, 1
  },
};
