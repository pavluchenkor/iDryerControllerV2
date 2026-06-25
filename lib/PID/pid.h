
#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <Arduino.h>

typedef enum {
  PID_ACT_HEATER = 0,   // нагреватель
  PID_ACT_AIR    = 1,   // воздух/вентилятор/охлаждение
  PID_ACT_OTHER  = 2
} PID_Actuator;

static inline const char* pid_actuator_str(PID_Actuator a) {
  switch (a) {
    case PID_ACT_HEATER: return "heater";
    case PID_ACT_AIR:    return "air";
    default:             return "other";
  }
}
typedef struct {
  // Идентификация
  uint8_t       id;              // номер контроллера/канала
  PID_Actuator  kind;            // тип контура: heater/air/other

  // Коэффициенты контроллера (параллельная форма)
  float Kp, Ki, Kd;                // П, И, Д

  float output_min, output_max;    // Диапазон управляющего сигнала (например, 0..100 %)
  float err_prev;                  // Предыдущее измерение
  float time_prev;                 // Время предыдущего шага (сек)
  float value_prev;                 

  float proportional;              // P‑слагаемое (телеметрия)
  float integral;                  // Интеграл ошибки
  float integral_prev;             // Интеграл ошибки
  float deriv;                     // Производная по измерению (d(value)/dt)
  float deriv_prev;                // Измеренная производная
  float deriv_filter;                // Измеренная производная

  float output_prev;               // Последний выданный выход
  float filter_prev;

  float min_dt;                     // Мин. шаг времени для расчёта производной
  float gain_multiplier;             // Лимит интеграла (anti‑windup)

  bool d_on_measurement;            
  // Телеметрия/служебное
  bool  output_updated;             // Выход обновлён на этом шаге
  uint16_t debug_count;             // Счётчик вызовов Compute (для логов)
  
} PID_Controller;

#ifdef __cplusplus
extern "C" {
#endif

void PID_Init(PID_Controller *pid,
                uint8_t id,
                PID_Actuator kind,
                float Kp, float Ki, float Kd,
                float out_min, float out_max,
                float min_dt, float gain_multiplier);

void PID_UpdateGain(PID_Controller *pid, float gain_multiplier, float Kp);

float PID_Compute(PID_Controller *pid, float value, float target, float now);
float clampf(float value, float min, float max);
void PID_ResetState(PID_Controller *pid);

//сеттеры
void PID_SetDerivFilterAlpha(PID_Controller *pid, float alpha);
void PID_SetSlew(PID_Controller *pid, float slew_per_sec);
void PID_SetOutputWindow(PID_Controller *pid, float out_min, float out_max,
                         float min_on, float off_thresh);
void PID_SetTaperBand(PID_Controller *pid, float band_deg);
void PID_SetDerivFilterTime(PID_Controller *pid, float T_sec);

/**
 * ВНИМАНИЕ: В проекте сейчас расчёт автотюна использует «classic Ziegler–Nichols»
 * (см. control.cpp: ZN Kp=0.6Ku, Ti=0.5Tu, Td=0.125Tu). Ниже — альтернативные
 * хелперы для Tyreus–Luyben (TL), не меняющие поведение существующего кода.
 *
 * TL (heater, внутренний контур):
 *   Kp = 0.31*Ku, Ti = 2.2*Tu, Td = 0.168*Tu → Ki = Kp/Ti, Kd = Kp*Td
 * TL (air, внешний контур, консервативнее):
 *   масштабируем Kp (и, как следствие, Ki/Kd) коэффициентом scale 0.5..0.7
 */

static inline void PID_Tune_TL_heater(float Ku, float Tu,
                                      float* outKp, float* outKi, float* outKd)
{
  const float Kp = 0.31f * Ku;
  const float Ti = 2.2f  * Tu;
  const float Td = 0.168f * Tu;
  const float Ki = (Ti > 0.0f) ? (Kp / Ti) : 0.0f;
  const float Kd = Kp * Td;
  if (outKp) *outKp = Kp;
  if (outKi) *outKi = Ki;
  if (outKd) *outKd = Kd;
}

static inline void PID_Tune_TL_air(float Ku, float Tu, float scale,
                                   float* outKp, float* outKi, float* outKd)
{
  if (scale <= 0.0f) scale = 0.6f; // типично 0.5..0.7
  const float Kp0 = 0.31f * Ku * scale;
  const float Ti  = 2.2f  * Tu;
  const float Td  = 0.168f * Tu;
  const float Ki  = (Ti > 0.0f) ? (Kp0 / Ti) : 0.0f;
  const float Kd  = Kp0 * Td;
  if (outKp) *outKp = Kp0;
  if (outKi) *outKi = Ki;
  if (outKd) *outKd = Kd;
}


#ifdef __cplusplus
}
#endif
#endif
