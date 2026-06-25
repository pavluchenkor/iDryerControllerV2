#include "pid.h"
#include <math.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_PID
#define LOG_TAG "PID"
#include "debug_log.h"


float clampf(float value, float minv, float maxv) {
  if (value < minv) return minv;
  if (value > maxv) return maxv;
  return value;
}


/**
 * @brief Инициализация PID-контроллера.
 *
 * @param pid              Указатель на структуру PID_Controller.
 * @param Kp               Пропорциональный коэффициент (P).
 * @param Ki               Интегральный коэффициент (I).
 * @param Kd               Дифференциальный коэффициент (D).
 * @param out_min          Минимальное значение выходного сигнала.
 * @param out_max          Максимальное значение выходного сигнала.
 * @param min_dt           Минимальное время между расчётами производной (секунды).
 * @param max_power        Максимальная мощность (используется для ограничения интеграла).
 */
void PID_Init(PID_Controller *pid, uint8_t id, PID_Actuator kind, float Kp, float Ki, float Kd, float out_min, float out_max, float min_dt, float gain_multiplier) {
  if (pid == NULL) return;
  pid->id = id;
  pid->kind = kind;
  pid->Kp = Kp;
  pid->Ki = Ki;
  pid->Kd = Kd;
  pid->output_min = out_min;
  pid->output_max = out_max;
  pid->min_dt = min_dt;
  pid->gain_multiplier = (Kp != 0.0f) ? (gain_multiplier / Kp) : 1.0f;
  PID_ResetState(pid);
}

void PID_UpdateGain(PID_Controller *pid, float gain_multiplier, float Kp) {
  if (pid == NULL) return;
  pid->gain_multiplier = (Kp != 0.0f) ? (gain_multiplier / Kp) : 1.0f;
}

float PID_Compute(PID_Controller *pid, float value, float target, float now) {
  // pid->Kp = 9.948f / 4.0f;
  // pid->Ki = 0.1f / 4.0f;
  // pid->Kd = 90.497f / 4.0f;

  float gm = pid->gain_multiplier;
  pid->d_on_measurement = true;
  pid->deriv_filter = 0.1f;

  float time_constant = pid->min_dt;
  pid->output_updated = false;

  float dt = now - pid->time_prev;

  if (dt < 0.0f) {
    PID_ResetState(pid);
    return pid->output_prev;
  }

  if (dt < pid->min_dt) {
    return pid->output_prev;
  }

  //! Рабочий вариант простейшего ПИД
  float err = target - value;
  pid->proportional = err;

  float deriv_filter_scale = 4.0;
  float tau_eff = time_constant * deriv_filter_scale;
  pid->time_prev = now;
  float a = 1.0f + 2.0f * tau_eff / dt;
  float aPrev = 1.0f - 2.0f * tau_eff / dt;
  float b = 2.0f / dt;
  float bPrev = -2.0f / dt;

  float x_now, x_prev;
  if (pid->d_on_measurement) {
    x_now = value;
    x_prev = pid->value_prev;
  } else {
    x_now = err;
    x_prev = pid->err_prev;
  }
  float filter_now = (x_now * b + x_prev * bPrev - pid->filter_prev * aPrev) / a;
  pid->filter_prev = filter_now;
  if (pid->d_on_measurement)
    pid->value_prev = value;
  else
    pid->err_prev = err;
  pid->deriv = pid->Kd * gm * (pid->d_on_measurement ? -filter_now : filter_now);

  // integral
  float u_raw = pid->Kp * gm * err + pid->integral + pid->deriv;
  float u_sat = clampf(u_raw, pid->output_min, pid->output_max);

  bool sat_hi = (u_raw > pid->output_max);
  bool sat_lo = (u_raw < pid->output_min);
  bool saturating = sat_hi || sat_lo;

  bool allow_integrate = !saturating || (sat_hi && (err < 0)) // упор вверх, ошибка тянет вниз
                         || (sat_lo && (err > 0));            // упор вниз, ошибка тянет вверх

  if (allow_integrate) {
    pid->integral += err * dt * pid->Ki * gm;
  }
  // const float Kaw = 1.5f;
  float Kaw = 2.0f; // 1.5
  if (pid->Ki > 0.0f) {
    // pid->integral += (u_sat - u_raw) * Kaw * dt;
    float tau_aw = fmaxf((3.0f * dt), 1.0e-3f);
    Kaw = 1.0f / tau_aw;
    // if (allow_integrate) {
    //   pid->integral += err * dt * pid->Ki * gm;
    // }
    float aw = (u_sat - u_raw) * Kaw * dt;
    // Применяем только если уменьшает |integral| (не толкаем интеграл в неверную сторону)
    if (aw * pid->integral < 0.0f) {
      pid->integral += aw;
    }
  } else {
    pid->integral = 0.0f;
  }

  float output = pid->proportional * pid->Kp * gm + pid->integral + pid->deriv;

  output = clampf(output, pid->output_min, pid->output_max);
  pid->output_prev = output;
  if (pid->kind == PID_ACT_AIR) {
    DEBUG_I("[%2u][%-6s]  tgt:%8.3f  Tc:%6.3f  dt:%6.3f"
            "  Kp*:%7.3f  Ki*:%7.3f  Kd*:%7.3f"
            "  err:% 8.3f  I:% 8.3f  D:% 8.3f"
            "  t:%7.2f  out:%7.2f",
            (unsigned)pid->id,           // [%2u]  -> id по ширине 2
            pid_actuator_str(pid->kind), // [%-6s] -> метка фикс. ширины 6: "air   ", "heater"
            (double)target, (double)time_constant, (double)dt, (double)(pid->Kp * gm), (double)(pid->Ki * gm), (double)(pid->Kd * gm), (double)err, (double)pid->integral, (double)pid->deriv, (double)value, (double)output);
  }
  return output;
}

void PID_ResetState(PID_Controller *pid) {
  // Жёсткий сброс всего состояния (как начальная инициализация)
  pid->proportional = 0.0f;
  pid->integral = 0.0f;
  pid->deriv = 0.0f;
  pid->err_prev = 0.0f;
  pid->time_prev = 0.0f;
  pid->deriv_prev = 0.0f;
  pid->output_updated = true;
}
