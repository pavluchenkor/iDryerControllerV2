// AUTO-GENERATED. DO NOT EDIT.
#include <string.h>
#include "menu_bindings.h"
#include "menu_eeprom_io.h"

#ifdef __cplusplus
extern "C" {
#endif

// forward decl for optional on_change
void set_active_controller(void* ctx); // C++ linkage
// forward decl for optional on_change
void scales_count_set(void* ctx); // C++ linkage
// forward decl for optional on_change
void pid_recalc_heater(void* ctx); // C++ linkage
// forward decl for optional on_change
void pid_recalc_gain_heater(void* ctx); // C++ linkage
// forward decl for optional on_change
void pid_recalc_chamber(void* ctx); // C++ linkage
// forward decl for optional on_change
void pid_recalc_gain_chamber(void* ctx); // C++ linkage
// forward decl for optional on_change
void on_servo_preview(void* ctx); // C++ linkage
// forward decl for optional on_change
void ws_toggle(void* ctx); // C++ linkage
// forward decl for optional on_change
void units_count_set(void* ctx); // C++ linkage
// forward decl for optional on_change
void on_language_changed(void* ctx); // C++ linkage
#ifdef __cplusplus
}
#endif

extern MenuState menu;
const MenuBinding g_bindings[] = {
  {MENU_CONTROLLER_CHOICE, "number_controller", VT_U8, (void*)&menu.number_controller, true, set_active_controller, false, SCOPE_GLOBAL, 2220, 0},
  {MENU_DRY_TEMP, "dry_temp", VT_F32, (void*)&menu.dry_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2221, 4},
  {MENU_DRY_TIME, "dry_time", VT_U16, (void*)&menu.dry_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2233, 2},
  {MENU_STORAGE_TEMP, "storage_temp", VT_U8, (void*)&menu.storage_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2239, 1},
  {MENU_STORAGE_HUM, "storage_hum", VT_U8, (void*)&menu.storage_hum, true, nullptr, false, SCOPE_PER_CONTROLLER, 2242, 1},
  {MENU_STORAGE_HUM_PRIORITY, "storage_hum_priority", VT_BOOL, (void*)&menu.storage_hum_priority, true, nullptr, false, SCOPE_PER_CONTROLLER, 2245, 1},
  {MENU_START_STAGE_BY_NUMBER, "start_stage_by_number", VT_U8, (void*)&menu.start_stage_by_number, true, nullptr, false, SCOPE_PER_CONTROLLER, 2248, 1},
  {MENU_STAGE_01_TEMP, "profile_stage_01_temp", VT_U8, (void*)&menu.profile_stage_01_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2251, 1},
  {MENU_STAGE_01_RAMPS, "profile_stage_01_ramps_time", VT_U16, (void*)&menu.profile_stage_01_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2254, 2},
  {MENU_STAGE_01_HOLD, "profile_stage_01_time", VT_U16, (void*)&menu.profile_stage_01_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2260, 2},
  {MENU_STAGE_02_TEMP, "profile_stage_02_temp", VT_U8, (void*)&menu.profile_stage_02_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2266, 1},
  {MENU_STAGE_02_RAMPS, "profile_stage_02_ramps_time", VT_U16, (void*)&menu.profile_stage_02_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2269, 2},
  {MENU_STAGE_02_HOLD, "profile_stage_02_time", VT_U16, (void*)&menu.profile_stage_02_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2275, 2},
  {MENU_STAGE_03_TEMP, "profile_stage_03_temp", VT_U8, (void*)&menu.profile_stage_03_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2281, 1},
  {MENU_STAGE_03_RAMPS, "profile_stage_03_ramps_time", VT_U16, (void*)&menu.profile_stage_03_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2284, 2},
  {MENU_STAGE_03_HOLD, "profile_stage_03_time", VT_U16, (void*)&menu.profile_stage_03_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2290, 2},
  {MENU_STAGE_04_TEMP, "profile_stage_04_temp", VT_U8, (void*)&menu.profile_stage_04_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2296, 1},
  {MENU_STAGE_04_RAMPS, "profile_stage_04_ramps_time", VT_U16, (void*)&menu.profile_stage_04_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2299, 2},
  {MENU_STAGE_04_HOLD, "profile_stage_04_time", VT_U16, (void*)&menu.profile_stage_04_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2305, 2},
  {MENU_STAGE_05_TEMP, "profile_stage_05_temp", VT_U8, (void*)&menu.profile_stage_05_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2311, 1},
  {MENU_STAGE_05_RAMPS, "profile_stage_05_ramps_time", VT_U16, (void*)&menu.profile_stage_05_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2314, 2},
  {MENU_STAGE_05_HOLD, "profile_stage_05_time", VT_U16, (void*)&menu.profile_stage_05_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2320, 2},
  {MENU_STAGE_06_TEMP, "profile_stage_06_temp", VT_U8, (void*)&menu.profile_stage_06_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2326, 1},
  {MENU_STAGE_06_RAMPS, "profile_stage_06_ramps_time", VT_U16, (void*)&menu.profile_stage_06_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2329, 2},
  {MENU_STAGE_06_HOLD, "profile_stage_06_time", VT_U16, (void*)&menu.profile_stage_06_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2335, 2},
  {MENU_STAGE_07_TEMP, "profile_stage_07_temp", VT_U8, (void*)&menu.profile_stage_07_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2341, 1},
  {MENU_STAGE_07_RAMPS, "profile_stage_07_ramps_time", VT_U16, (void*)&menu.profile_stage_07_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2344, 2},
  {MENU_STAGE_07_HOLD, "profile_stage_07_time", VT_U16, (void*)&menu.profile_stage_07_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2350, 2},
  {MENU_STAGE_08_TEMP, "profile_stage_08_temp", VT_U8, (void*)&menu.profile_stage_08_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2356, 1},
  {MENU_STAGE_08_RAMPS, "profile_stage_08_ramps_time", VT_U16, (void*)&menu.profile_stage_08_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2359, 2},
  {MENU_STAGE_08_HOLD, "profile_stage_08_time", VT_U16, (void*)&menu.profile_stage_08_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2365, 2},
  {MENU_STAGE_09_TEMP, "profile_stage_09_temp", VT_U8, (void*)&menu.profile_stage_09_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2371, 1},
  {MENU_STAGE_09_RAMPS, "profile_stage_09_ramps_time", VT_U16, (void*)&menu.profile_stage_09_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2374, 2},
  {MENU_STAGE_09_HOLD, "profile_stage_09_time", VT_U16, (void*)&menu.profile_stage_09_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2380, 2},
  {MENU_STAGE_10_TEMP, "profile_stage_10_temp", VT_U8, (void*)&menu.profile_stage_10_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2386, 1},
  {MENU_STAGE_10_RAMPS, "profile_stage_10_ramps_time", VT_U16, (void*)&menu.profile_stage_10_ramps_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2389, 2},
  {MENU_STAGE_10_HOLD, "profile_stage_10_time", VT_U16, (void*)&menu.profile_stage_10_time, true, nullptr, false, SCOPE_PER_CONTROLLER, 2395, 2},
  {MENU_PRESET_PLA_TEMP, "preset_pla_temp", VT_F32, (void*)&menu.preset_pla_temp, true, nullptr, false, SCOPE_GLOBAL, 2401, 0},
  {MENU_PRESET_PLA_TIME, "preset_pla_time", VT_U16, (void*)&menu.preset_pla_time, true, nullptr, false, SCOPE_GLOBAL, 2405, 0},
  {MENU_PRESET_PLA_CF_TEMP, "preset_pla_cf_temp", VT_F32, (void*)&menu.preset_pla_cf_temp, true, nullptr, false, SCOPE_GLOBAL, 2407, 0},
  {MENU_PRESET_PLA_CF_TIME, "preset_pla_cf_time", VT_U16, (void*)&menu.preset_pla_cf_time, true, nullptr, false, SCOPE_GLOBAL, 2411, 0},
  {MENU_PRESET_PLA_GF_TEMP, "preset_pla_gf_temp", VT_F32, (void*)&menu.preset_pla_gf_temp, true, nullptr, false, SCOPE_GLOBAL, 2413, 0},
  {MENU_PRESET_PLA_GF_TIME, "preset_pla_gf_time", VT_U16, (void*)&menu.preset_pla_gf_time, true, nullptr, false, SCOPE_GLOBAL, 2417, 0},
  {MENU_PRESET_PETG_TEMP, "preset_petg_temp", VT_F32, (void*)&menu.preset_petg_temp, true, nullptr, false, SCOPE_GLOBAL, 2419, 0},
  {MENU_PRESET_PETG_TIME, "preset_petg_time", VT_U16, (void*)&menu.preset_petg_time, true, nullptr, false, SCOPE_GLOBAL, 2423, 0},
  {MENU_PRESET_PETG_CF_TEMP, "preset_petg_cf_temp", VT_F32, (void*)&menu.preset_petg_cf_temp, true, nullptr, false, SCOPE_GLOBAL, 2425, 0},
  {MENU_PRESET_PETG_CF_TIME, "preset_petg_cf_time", VT_U16, (void*)&menu.preset_petg_cf_time, true, nullptr, false, SCOPE_GLOBAL, 2429, 0},
  {MENU_PRESET_PETG_GF_TEMP, "preset_petg_gf_temp", VT_F32, (void*)&menu.preset_petg_gf_temp, true, nullptr, false, SCOPE_GLOBAL, 2431, 0},
  {MENU_PRESET_PETG_GF_TIME, "preset_petg_gf_time", VT_U16, (void*)&menu.preset_petg_gf_time, true, nullptr, false, SCOPE_GLOBAL, 2435, 0},
  {MENU_PRESET_ABS_TEMP, "preset_abs_temp", VT_F32, (void*)&menu.preset_abs_temp, true, nullptr, false, SCOPE_GLOBAL, 2437, 0},
  {MENU_PRESET_ABS_TIME, "preset_abs_time", VT_U16, (void*)&menu.preset_abs_time, true, nullptr, false, SCOPE_GLOBAL, 2441, 0},
  {MENU_PRESET_ABS_CF_TEMP, "preset_abs_cf_temp", VT_F32, (void*)&menu.preset_abs_cf_temp, true, nullptr, false, SCOPE_GLOBAL, 2443, 0},
  {MENU_PRESET_ABS_CF_TIME, "preset_abs_cf_time", VT_U16, (void*)&menu.preset_abs_cf_time, true, nullptr, false, SCOPE_GLOBAL, 2447, 0},
  {MENU_PRESET_ABS_GF_TEMP, "preset_abs_gf_temp", VT_F32, (void*)&menu.preset_abs_gf_temp, true, nullptr, false, SCOPE_GLOBAL, 2449, 0},
  {MENU_PRESET_ABS_GF_TIME, "preset_abs_gf_time", VT_U16, (void*)&menu.preset_abs_gf_time, true, nullptr, false, SCOPE_GLOBAL, 2453, 0},
  {MENU_PRESET_PA_TEMP, "preset_pa_temp", VT_F32, (void*)&menu.preset_pa_temp, true, nullptr, false, SCOPE_GLOBAL, 2455, 0},
  {MENU_PRESET_PA_TIME, "preset_pa_time", VT_U16, (void*)&menu.preset_pa_time, true, nullptr, false, SCOPE_GLOBAL, 2459, 0},
  {MENU_PRESET_PA_CF_TEMP, "preset_pa_cf_temp", VT_F32, (void*)&menu.preset_pa_cf_temp, true, nullptr, false, SCOPE_GLOBAL, 2461, 0},
  {MENU_PRESET_PA_CF_TIME, "preset_pa_cf_time", VT_U16, (void*)&menu.preset_pa_cf_time, true, nullptr, false, SCOPE_GLOBAL, 2465, 0},
  {MENU_PRESET_PA_GF_TEMP, "preset_pa_gf_temp", VT_F32, (void*)&menu.preset_pa_gf_temp, true, nullptr, false, SCOPE_GLOBAL, 2467, 0},
  {MENU_PRESET_PA_GF_TIME, "preset_pa_gf_time", VT_U16, (void*)&menu.preset_pa_gf_time, true, nullptr, false, SCOPE_GLOBAL, 2471, 0},
  {MENU_PRESET_PC_TEMP, "preset_pc_temp", VT_F32, (void*)&menu.preset_pc_temp, true, nullptr, false, SCOPE_GLOBAL, 2473, 0},
  {MENU_PRESET_PC_TIME, "preset_pc_time", VT_U16, (void*)&menu.preset_pc_time, true, nullptr, false, SCOPE_GLOBAL, 2477, 0},
  {MENU_PRESET_PC_CF_TEMP, "preset_pc_cf_temp", VT_F32, (void*)&menu.preset_pc_cf_temp, true, nullptr, false, SCOPE_GLOBAL, 2479, 0},
  {MENU_PRESET_PC_CF_TIME, "preset_pc_cf_time", VT_U16, (void*)&menu.preset_pc_cf_time, true, nullptr, false, SCOPE_GLOBAL, 2483, 0},
  {MENU_PRESET_MY1_TEMP, "preset_my1_temp", VT_F32, (void*)&menu.preset_my1_temp, true, nullptr, false, SCOPE_GLOBAL, 2485, 0},
  {MENU_PRESET_MY1_TIME, "preset_my1_time", VT_U16, (void*)&menu.preset_my1_time, true, nullptr, false, SCOPE_GLOBAL, 2489, 0},
  {MENU_PRESET_MY2_TEMP, "preset_my2_temp", VT_F32, (void*)&menu.preset_my2_temp, true, nullptr, false, SCOPE_GLOBAL, 2491, 0},
  {MENU_PRESET_MY2_TIME, "preset_my2_time", VT_U16, (void*)&menu.preset_my2_time, true, nullptr, false, SCOPE_GLOBAL, 2495, 0},
  {MENU_PRESET_MY3_TEMP, "preset_my3_temp", VT_F32, (void*)&menu.preset_my3_temp, true, nullptr, false, SCOPE_GLOBAL, 2497, 0},
  {MENU_PRESET_MY3_TIME, "preset_my3_time", VT_U16, (void*)&menu.preset_my3_time, true, nullptr, false, SCOPE_GLOBAL, 2501, 0},
  {MENU_SCALES_COUNT, "scales_count", VT_U8, (void*)&menu.scales_count, true, scales_count_set, false, SCOPE_GLOBAL, 2503, 0},
  {MENU_TARE_SPOOL1, "tare_spool1", VT_F32, (void*)&menu.tare_spool1, true, nullptr, false, SCOPE_GLOBAL, 2504, 0},
  {MENU_TARE_SPOOL2, "tare_spool2", VT_F32, (void*)&menu.tare_spool2, true, nullptr, false, SCOPE_GLOBAL, 2508, 0},
  {MENU_TARE_SPOOL3, "tare_spool3", VT_F32, (void*)&menu.tare_spool3, true, nullptr, false, SCOPE_GLOBAL, 2512, 0},
  {MENU_TARE_SPOOL4, "tare_spool4", VT_F32, (void*)&menu.tare_spool4, true, nullptr, false, SCOPE_GLOBAL, 2516, 0},
  {MENU_STORAGE_AUTO, "storage_auto", VT_BOOL, (void*)&menu.storage_auto, true, nullptr, false, SCOPE_PER_CONTROLLER, 2520, 1},
  {MENU_STORAGE_AUTO_DRY, "storage_auto_dry", VT_BOOL, (void*)&menu.storage_auto_dry, true, nullptr, false, SCOPE_PER_CONTROLLER, 2523, 1},
  {MENU_STORAGE_RH_HYST, "storage_rh_hyst", VT_U8, (void*)&menu.storage_rh_hyst, true, nullptr, false, SCOPE_PER_CONTROLLER, 2526, 1},
  {MENU_STORAGE_DRY_TEMP_MODE, "storage_dry_temp_mode", VT_BOOL, (void*)&menu.storage_dry_temp_mode, true, nullptr, false, SCOPE_PER_CONTROLLER, 2529, 1},
  {MENU_STORAGE_DRY_TEMP_BY_DRY_TEMP, "storage_dry_temp_by_dry_temp", VT_U8, (void*)&menu.storage_dry_temp_by_dry_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2532, 1},
  {MENU_STORAGE_MIN_HOLD_SEC, "storage_min_hold_sec", VT_U16, (void*)&menu.storage_min_hold_sec, true, nullptr, false, SCOPE_PER_CONTROLLER, 2535, 2},
  {MENU_PID_KP_HEATER, "pid_kp_heater", VT_F32, (void*)&menu.pid_kp_heater, true, pid_recalc_heater, false, SCOPE_PER_CONTROLLER, 2541, 4},
  {MENU_PID_KI_HEATER, "pid_ki_heater", VT_F32, (void*)&menu.pid_ki_heater, true, pid_recalc_heater, false, SCOPE_PER_CONTROLLER, 2553, 4},
  {MENU_PID_KD_HEATER, "pid_kd_heater", VT_F32, (void*)&menu.pid_kd_heater, true, pid_recalc_heater, false, SCOPE_PER_CONTROLLER, 2565, 4},
  {MENU_PID_GAIN_HEATER, "pid_gain_heater", VT_F32, (void*)&menu.pid_gain_heater, true, pid_recalc_gain_heater, false, SCOPE_PER_CONTROLLER, 2577, 4},
  {MENU_PID_KP_CHAMBER, "pid_kp_chamber", VT_F32, (void*)&menu.pid_kp_chamber, true, pid_recalc_chamber, false, SCOPE_PER_CONTROLLER, 2589, 4},
  {MENU_PID_KI_CHAMBER, "pid_ki_chamber", VT_F32, (void*)&menu.pid_ki_chamber, true, pid_recalc_chamber, false, SCOPE_PER_CONTROLLER, 2601, 4},
  {MENU_PID_KD_CHAMBER, "pid_kd_chamber", VT_F32, (void*)&menu.pid_kd_chamber, true, pid_recalc_chamber, false, SCOPE_PER_CONTROLLER, 2613, 4},
  {MENU_PID_GAIN_CHAMBER, "pid_gain_chamber", VT_F32, (void*)&menu.pid_gain_chamber, true, pid_recalc_gain_chamber, false, SCOPE_PER_CONTROLLER, 2625, 4},
  {MENU_HEATER_MAX_TEMP, "heater_max_temp", VT_F32, (void*)&menu.heater_max_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2637, 4},
  {MENU_AIR_MAX_TEMP, "air_max_temp", VT_F32, (void*)&menu.air_max_temp, true, nullptr, false, SCOPE_PER_CONTROLLER, 2649, 4},
  {MENU_DELTA, "delta_c", VT_U8, (void*)&menu.delta_c, true, nullptr, false, SCOPE_PER_CONTROLLER, 2661, 1},
  {MENU_HEATER_DELTA_IS_PERCENT, "heater_delta_is_percent", VT_BOOL, (void*)&menu.heater_delta_is_percent, true, nullptr, false, SCOPE_PER_CONTROLLER, 2664, 1},
  {MENU_VH_MAX_ERR, "vh_max_err", VT_F32, (void*)&menu.vh_max_err, true, nullptr, false, SCOPE_PER_CONTROLLER, 2667, 4},
  {MENU_VH_HEATING_GAIN, "vh_heating_gain", VT_F32, (void*)&menu.vh_heating_gain, true, nullptr, false, SCOPE_PER_CONTROLLER, 2679, 4},
  {MENU_VH_CHECK_GAIN_TIME, "vh_check_gain_time_s", VT_U16, (void*)&menu.vh_check_gain_time_s, true, nullptr, false, SCOPE_PER_CONTROLLER, 2691, 2},
  {MENU_VH_ARM_PWM_MIN, "vh_arm_pwm_min_pct", VT_U8, (void*)&menu.vh_arm_pwm_min_pct, true, nullptr, false, SCOPE_PER_CONTROLLER, 2697, 1},
  {MENU_FAN_TEMP_ON, "fan_temp_on_c", VT_F32, (void*)&menu.fan_temp_on_c, true, nullptr, false, SCOPE_PER_CONTROLLER, 2700, 4},
  {MENU_FAN_HYST, "fan_hyst_c", VT_F32, (void*)&menu.fan_hyst_c, true, nullptr, false, SCOPE_PER_CONTROLLER, 2712, 4},
  {MENU_SERVO_CLOSED_ANGLE, "servo_closed_angle", VT_U8, (void*)&menu.servo_closed_angle, true, on_servo_preview, false, SCOPE_PER_CONTROLLER, 2724, 1},
  {MENU_SERVO_OPEN_ANGLE, "servo_open_angle", VT_U8, (void*)&menu.servo_open_angle, true, on_servo_preview, false, SCOPE_PER_CONTROLLER, 2727, 1},
  {MENU_SERVO_TIME_CLOSED, "servo_time_closed", VT_U16, (void*)&menu.servo_time_closed, true, nullptr, false, SCOPE_PER_CONTROLLER, 2730, 2},
  {MENU_SERVO_TIME_OPEN, "servo_time_open", VT_U16, (void*)&menu.servo_time_open, true, nullptr, false, SCOPE_PER_CONTROLLER, 2736, 2},
  {MENU_SERVO_SMART_MODE, "servo_smart_mode", VT_BOOL, (void*)&menu.servo_smart_mode, true, nullptr, false, SCOPE_PER_CONTROLLER, 2742, 1},
  {MENU_IGNORE_EXTERNAL_CMD, "ign_ext_cmd", VT_BOOL, (void*)&menu.ign_ext_cmd, true, nullptr, false, SCOPE_GLOBAL, 2745, 0},
  {MENU_DRYING_SESSION_COUNT, "drying_session_count", VT_U16, (void*)&menu.drying_session_count, true, nullptr, false, SCOPE_GLOBAL, 2746, 0},
  {MENU_STORAGE_SESSION_COUNT, "storage_session_count", VT_U16, (void*)&menu.storage_session_count, true, nullptr, false, SCOPE_GLOBAL, 2748, 0},
  {MENU_PROFILE_SESSION_COUNT, "profile_session_count", VT_U16, (void*)&menu.profile_session_count, true, nullptr, false, SCOPE_GLOBAL, 2750, 0},
  {MENU_PORT1_MODE, "port1_mode", VT_U8, (void*)&menu.port1_mode, true, nullptr, false, SCOPE_GLOBAL, 2752, 0},
  {MENU_PORT2_MODE, "port2_mode", VT_U8, (void*)&menu.port2_mode, true, nullptr, false, SCOPE_GLOBAL, 2753, 0},
  {MENU_PORT3_MODE, "port3_mode", VT_U8, (void*)&menu.port3_mode, true, nullptr, false, SCOPE_GLOBAL, 2754, 0},
  {MENU_WS_ENABLED, "ws_enabled", VT_BOOL, (void*)&menu.ws_enabled, true, ws_toggle, false, SCOPE_GLOBAL, 2755, 0},
  {MENU_UNITS_COUNT, "units_count", VT_U8, (void*)&menu.units_count, true, units_count_set, false, SCOPE_GLOBAL, 2756, 0},
  {MENU_LANGUAGE, "language", VT_U8, (void*)&menu.language, true, on_language_changed, true, SCOPE_GLOBAL, 2757, 0},
};
const uint16_t g_bindings_count = sizeof(g_bindings)/sizeof(g_bindings[0]);

// Global config change hook
ConfigChangeHookFn g_config_change_hook = nullptr;

void menu_set_config_change_hook(ConfigChangeHookFn hook) {
  g_config_change_hook = hook;
}

const MenuBinding* menu_find_bind(const char* bind) {
  if (!bind) return nullptr;
  for (uint16_t i=0;i<g_bindings_count;i++)
    if (strcmp(g_bindings[i].bind, bind) == 0) return &g_bindings[i];
  return nullptr;
}

static inline void store_value(const MenuBinding& b, float v, uint8_t idx) {
  switch (b.vtype) {
    case VT_F32:  if (b.scope==SCOPE_GLOBAL) *(float*)b.ptr    = v; else ((float*)b.ptr)[idx]    = v; break;
    case VT_U16:  if (b.scope==SCOPE_GLOBAL) *(uint16_t*)b.ptr = (uint16_t)v; else ((uint16_t*)b.ptr)[idx] = (uint16_t)v; break;
    case VT_U8:   if (b.scope==SCOPE_GLOBAL) *(uint8_t*)b.ptr  = (uint8_t)v;  else ((uint8_t*)b.ptr)[idx]  = (uint8_t)v;  break;
    case VT_I32:  if (b.scope==SCOPE_GLOBAL) *(int32_t*)b.ptr  = (int32_t)v;  else ((int32_t*)b.ptr)[idx]  = (int32_t)v;  break;
    case VT_BOOL: if (b.scope==SCOPE_GLOBAL) *(bool*)b.ptr     = (bool)(v!=0.0f); else ((bool*)b.ptr)[idx]     = (bool)(v!=0.0f); break;
    case VT_U32:  if (b.scope==SCOPE_GLOBAL) *(uint32_t*)b.ptr = (uint32_t)v; else ((uint32_t*)b.ptr)[idx] = (uint32_t)v; break;
  }
}

static inline void read_value(const MenuBinding& b, void* out_value, uint8_t idx) {
  switch (b.vtype) {
    case VT_F32:  *(float*)out_value    = (b.scope==SCOPE_GLOBAL)? *(float*)b.ptr    : ((float*)b.ptr)[idx]; break;
    case VT_U16:  *(uint16_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint16_t*)b.ptr : ((uint16_t*)b.ptr)[idx]; break;
    case VT_U8:   *(uint8_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(uint8_t*)b.ptr  : ((uint8_t*)b.ptr)[idx]; break;
    case VT_I32:  *(int32_t*)out_value  = (b.scope==SCOPE_GLOBAL)? *(int32_t*)b.ptr  : ((int32_t*)b.ptr)[idx]; break;
    case VT_BOOL: *(bool*)out_value     = (b.scope==SCOPE_GLOBAL)? *(bool*)b.ptr     : ((bool*)b.ptr)[idx]; break;
    case VT_U32:  *(uint32_t*)out_value = (b.scope==SCOPE_GLOBAL)? *(uint32_t*)b.ptr : ((uint32_t*)b.ptr)[idx]; break;
  }
}

static inline uint16_t calc_eeprom_offset(const MenuBinding& b, uint8_t idx){
  if (!b.persist) return 0;
  if (b.scope == SCOPE_GLOBAL) return b.ee_base;
  return (uint16_t)(b.ee_base + (uint16_t)idx * b.ee_stride);
}

bool menu_read_by_bind(const char* bind, void* out_value) {
  const MenuBinding* b = menu_find_bind(bind);
  if (!b || !out_value) return false;
  uint8_t idx = 0;
  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();
  read_value(*b, out_value, idx);
  return true;
}

bool menu_apply_by_bind(const char* bind, float v) {
  const MenuBinding* b = menu_find_bind(bind);
  if (!b) return false;
  uint8_t idx = 0;
  if (b->scope == SCOPE_PER_CONTROLLER) idx = menu_get_active_controller();
  store_value(*b, v, idx);
  if (b->persist) {
    uint16_t ee = calc_eeprom_offset(*b, idx);
    switch (b->vtype) {
      case VT_F32:  ee_store_field<float>(   ee, (b->scope==SCOPE_GLOBAL)? *(float*)b->ptr    : ((float*)b->ptr)[idx]); break;
      case VT_U16:  ee_store_field<uint16_t>(ee, (b->scope==SCOPE_GLOBAL)? *(uint16_t*)b->ptr : ((uint16_t*)b->ptr)[idx]); break;
      case VT_U8:   ee_store_field<uint8_t>( ee, (b->scope==SCOPE_GLOBAL)? *(uint8_t*)b->ptr  : ((uint8_t*)b->ptr)[idx]); break;
      case VT_I32:  ee_store_field<int32_t>( ee, (b->scope==SCOPE_GLOBAL)? *(int32_t*)b->ptr  : ((int32_t*)b->ptr)[idx]); break;
      case VT_BOOL: ee_store_field<bool>(    ee, (b->scope==SCOPE_GLOBAL)? *(bool*)b->ptr     : ((bool*)b->ptr)[idx]); break;
      case VT_U32:  ee_store_field<uint32_t>(ee, (b->scope==SCOPE_GLOBAL)? *(uint32_t*)b->ptr : ((uint32_t*)b->ptr)[idx]); break;
    }
  }
  if (b->on_change) b->on_change((void*)b->ptr);
  if (g_config_change_hook) g_config_change_hook(b->id, idx, b->bind);
  return true;
}
