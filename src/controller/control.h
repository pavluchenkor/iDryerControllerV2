#pragma once
#include "HX711/HX711.h"
#include "configuration.h"
#include <Arduino.h>
#include <Servo.h>
#include <stdint.h>

extern "C" {
#include "pid.h"
}

#if defined(ARDUINO_ARCH_RP2040)
#include "SoftPWM/soft_pwm.h"
#define USE_SOFT_PWM
#endif

#ifndef HEATER_OFF_LEVEL
#define HEATER_OFF_LEVEL LOW
#endif

// #define RH_HISTORY 10
// #define SAMPLE_PERIOD_MIN 10
// Минимальная скорость изменения RH, при которой считаем, что "что-то реально происходит"
#define DRH_THRESHOLD 0.2f // [%/мин]
#define RH_HIGH 40.0f      // [%] считаем, что в камере баня
#define RH_TREND_WINDOW 10

// ---- RH ring (без таймштампов) ----
#define RH_HISTORY_LEN 512
#define RH_SAVE_PERIOD_MS 500 // шаг записи RH

#define RH_TREND_UPDATE_MS 1000 // как часто считаем тренд (мс)
#define RH_TREND_TIME_S 30      //
#define RH_TREND_POINTS RH_TREND_TIME_S * 1000 / RH_SAVE_PERIOD_MS

struct RhRing {
  float buf[RH_HISTORY_LEN];
  int head = 0;
  int count = 0;
  uint32_t nextPushMs = 0;
};
struct RhTrend {
  float slope_per_min;
  float r2;
  float atr_per_min;
  int npts;
};
enum class TrendPhase : uint8_t { Unknown, Rising, Plateau, Falling };

struct TrendCfg {
  int min_points = RH_TREND_POINTS;
  float atr_floor = 0.1f;     // минимальный ATR для нормализации (защита от деления на 0), %RH/мин
  float z_thr_rise = 0.001f;  // 0.005f быстрее уходим в плато при замедлении роста
  float z_thr_fall = 0.0005f; // 0.001f держим Falling дольше (плату признаём, когда |Z| совсем мал)  float z_hyst          = 0.01f;  // небольшой гистерезис по Z, чтобы не дёргаться на границе
  uint32_t nextWin = 0;
};

enum class AutoTuneTarget : uint8_t { HeaterTemp, AirTemp };

struct PidAutoResult {
  bool ok = false;
  float Ku = 0, Tu = 0;         // предельный коэффициент и период
  float Kp = 0, Ki = 0, Kd = 0; // параллельная форма: u = Kp*e + Ki*∫e dt + Kd*de/dt
};

struct PidAutoState {
  static constexpr uint8_t MAX_PEAKS = 24;

  bool active = false;
  bool heating = true;
  AutoTuneTarget targetSel = AutoTuneTarget::HeaterTemp;

  // Параметры
  float targetC = 0.0f;      // целевая температура
  float bandC = 2.0f;        // полуширина зоны переключения (+-bandC)
  float maxPower01 = 1.0f;   // мощность в фазе нагрева
  float relayLow = 0.0f;     // нижний уровень релейного воздействия: power(0..1) для heater, heater setpoint(°C) для air
  float relayHigh = 0.0f;    // верхний уровень релейного воздействия: power(0..1) для heater, heater setpoint(°C) для air
  uint8_t needCycles = 12;   // сколько пиков (≈ как в Klipper)
  uint16_t minHoldMs = 1000; // антидребезг фазы

  // Данные
  uint8_t peaks = 0;
  float maxTemps[24]; // пик горячо
  float minTemps[24]; // впадина холодно
  uint32_t maxTimes[24];
  uint32_t minTimes[24];

  float lastTemp = NAN;
  uint32_t lastToggleMs = 0;
  uint32_t lastStepMs = 0; // для фиксированного шага тюнинга (гейтинг по времени)

  float curMax = -INFINITY, curMin = INFINITY; // трекинг текущего пика/впадины
  uint32_t curMaxTime = 0, curMinTime = 0;     // время достижения текущего экстремума
  uint8_t nMax = 0, nMin = 0;                  // сколько записали
  bool haveHot = false, haveCold = false;      // для полного цикла

  uint32_t resultShowUntilMs = 0;

  PidAutoResult result;
};

struct PidAutoView {
  bool active = false;
  bool heating = false;
  AutoTuneTarget targetSel = AutoTuneTarget::HeaterTemp;

  float targetC = 0.0f;
  float bandC = 0.0f;
  uint8_t peaks = 0, needCycles = 0;

  float currentTempC = NAN;   // что калибруем (heater/air)
  float duty01 = 0.0f;        // последняя поданная мощность 0..1
  uint32_t sinceToggleMs = 0; // сколько прошло с последнего переключения

  bool resultOk = false;
  float Kp = 0, Ki = 0, Kd = 0;

  uint32_t resultShowUntilMs;
};

// Profile mode structures
#define PROFILE_MAX_STAGES 10

struct ProfileStage {
  uint16_t holdTempC;     // целевая температура на HOLD (°C)
  uint16_t holdSeconds;   // время удержания (секунды)
  uint16_t rampSeconds;   // время рампа к следующему стейджу (секунды)
};

struct ProfileData {
  uint8_t stageCount;                     // количество стейджей (1-10)
  ProfileStage stages[PROFILE_MAX_STAGES];
};

struct ProfileState {
  uint8_t currentStage;      // 0-9, текущий стейдж во время выполнения
  enum class Phase : uint8_t { Hold, Ramp } phase = Phase::Hold;
  uint32_t phaseStartMs;     // когда началась текущая фаза
  uint32_t phaseDurationMs;  // длительность текущей фазы в ms
  float rampStartTempC;      // начальная температура для RAMP фазы
};

enum class DryerMode : uint8_t { Error, Idle, Drying, Storage, Profile, PidAutoTune };

// Данные от датчиков (снаружи)
struct DryerInputs {
  float airTempC = 0.0f;    // температура воздуха
  float airHumRH = 0.0f;    // влажность воздуха
  float heaterTempC = 0.0f; // температура нагревателя
};


/**
 * @class DryerController
 * @brief Контроллер сушилки: управление нагревателем, вентилятором и сервоприводом.
 *
 * Создаёт экземпляр контроллера и настраивает базовые параметры управления нагревателем:
 * пин, частоту и диапазон ШИМ, а также пины вентилятора и сервопривода.
 *
 * @param heaterPin       GPIO-пин (MCU) для управления нагревателем (`PWM`/ключ).
 * @param fanPin          GPIO-пин для вентилятора камеры (обычно on/off или `PWM`).
 * @param servoPin        GPIO-пин сервопривода заслонки/клапана (`PWM` для сервопривода).
 * @param heaterPwmRange  Диапазон ШИМ нагревателя (число отсчётов duty).
 *                        Пример: 255 (8-бит), 1023 (10-бит). Влияет на разрешение duty.
 * @param heaterPwmFreqHz Частота низкоуровневого ШИМ для нагревателя, Гц.
 *                        Используется как драйвер ключа; при тепловых нагрузках
 *                        часто комбинируется с оконным управлением (time-proportioning).
 * @param unitIndex       Индекс устройства в многоблочной системе (0 для одиночного).
 *
 * @note Желательно применять оконное управление (например, окно 8–12 с) поверх
 *       низкоуровневого ШИМ: PID выдаёт долю окна (0..1), а драйвер включает нагреватель
 *       на непрерывный интервал on_time = duty * window, используя внутренний ШИМ на 100%.
 *
 * @warning Убедитесь, что выбранная частота ШИМ и тип ключа (`MOSFET`/`SSR`) совместимы:
 *          механические реле не допускают частого переключения, а SSR (AC) работают на полупериодах сети.
 *
 * @see PID_Controller  для настройки Kp/Ki/Kd, slew-rate, min_on/off_thresh и taper-band.
 *
 */
class DryerController {
public:
  DryerController(uint8_t heaterPin, uint8_t fanPin, uint8_t servoPin,
                  uint16_t heaterPwmRange = 255, // TODO: убрать
                  uint32_t heaterPwmFreqHz = 5, uint8_t unitIndex = 0);

  void begin();
  void tick();
  uint8_t index() const { return unitIndex_; }
  void setAirPid(uint8_t controller, PID_Actuator kind, float kp, float ki, float kd, float minOut = 0.0f, float maxTempC = 100.0f, float sampleSec = 0.25f, float gain_multiplier = 3.0f);
  void setHeaterPid(uint8_t controller, PID_Actuator kind, float kp, float ki, float kd, float minOut = 0.0f, float maxTempC = 100.0f, float sampleSec = 0.25f, float gain_multiplier = 3.0f);
  void updateHeaterGain(float gain_multiplier);
  void updateAirGain(float gain_multiplier);
  void setPidUpdateIntervalMs(uint16_t ms) { pidUpdateIntervalMs_ = ms; }
  void resetPid();

  // Обновление углов сервы из меню
  void setServoLimits(uint8_t angleClosed, uint8_t angleOpen);
  // Датчики пушатся снаружи
  void setInputs(const DryerInputs &in) { inputs_ = in; }

  void startDrying(uint16_t tempC, uint16_t minutes);
  void startStorage(uint16_t tempC, uint8_t targetHum, bool storageByHum = true);
  void startProfile(const ProfileData &prof, uint8_t startFromStage = 0);
  void tickProfile(uint32_t nowMs);
  int16_t getStatusTargetTempC10() const {
    if (mode_ == DryerMode::Idle || mode_ == DryerMode::Error) return 0;
    return (int16_t)(targetTemp_ * 10);
  }
  uint16_t getStatusTargetHumidityPct() const {
    return (mode_ == DryerMode::Storage) ? targetHum_ : 0;
  }
  uint16_t getStatusDurationMinutes() const {
    if (mode_ == DryerMode::Drying) return (uint16_t)(dryTotalMs_ / 60000UL);
    if (mode_ == DryerMode::Profile) {
      uint32_t totalSeconds = getProfilePlannedTotalSeconds_();
      uint32_t minutes = totalSeconds / 60UL;
      return (minutes > 0xFFFFu) ? 0xFFFFu : (uint16_t)minutes;
    }
    return 0;
  }
  uint32_t getStatusElapsedSeconds(uint32_t nowMs) const {
    if (mode_ == DryerMode::Drying) {
      return (nowMs - dryModeStartMs_) / 1000UL;
    }
    if (mode_ == DryerMode::Profile) {
      return getProfileTotalElapsedSeconds(nowMs);
    }
    return 0;
  }
  uint32_t getStatusTotalRemainingSeconds(uint32_t nowMs) const {
    if (mode_ == DryerMode::Drying) {
      uint32_t total = dryTotalMs_ / 1000UL;
      if (dryStartMs_ == 0) return total;
      uint32_t elapsed = (nowMs - dryStartMs_) / 1000UL;
      return (elapsed >= total) ? 0 : (total - elapsed);
    }
    if (mode_ == DryerMode::Profile) {
      uint32_t total = getProfilePlannedTotalSeconds_();
      uint32_t elapsed = getProfileTotalElapsedSeconds(nowMs);
      return (elapsed >= total) ? 0 : (total - elapsed);
    }
    return 0;
  }

  uint8_t getProfileCurrentStage() const { return profileState_.currentStage; }
  uint8_t getProfileTotalStages() const { return profileData_.stageCount; }
  ProfileState::Phase getProfilePhase() const { return profileState_.phase; }
  uint32_t getProfileStageElapsedSeconds(uint32_t nowMs) const {
    if (mode_ != DryerMode::Profile || profileState_.currentStage >= profileData_.stageCount) return 0;
    return (nowMs - profileState_.phaseStartMs) / 1000UL;
  }
  uint32_t getProfileStageRemainingSeconds(uint32_t nowMs) const {
    if (mode_ != DryerMode::Profile || profileState_.currentStage >= profileData_.stageCount) return 0;
    const ProfileStage &st = profileData_.stages[profileState_.currentStage];
    uint32_t phaseTotalSec = (profileState_.phase == ProfileState::Phase::Ramp) ? st.rampSeconds : st.holdSeconds;
    uint32_t elapsedSec = getProfileStageElapsedSeconds(nowMs);
    return (elapsedSec >= phaseTotalSec) ? 0 : (phaseTotalSec - elapsedSec);
  }
  uint32_t getProfileTotalElapsedSeconds(uint32_t nowMs) const {
    if (mode_ != DryerMode::Profile || profileState_.currentStage >= profileData_.stageCount) return 0;

    uint32_t elapsed = getProfileCompletedPlannedSeconds_();
    if (profileState_.phase == ProfileState::Phase::Hold) {
      elapsed += profileData_.stages[profileState_.currentStage].rampSeconds;
    }
    elapsed += getProfileStageElapsedSeconds(nowMs);
    return elapsed;
  }
  uint32_t getProfilePhaseRemainingMs() const {
    uint32_t nowMs = millis();
    uint32_t elapsed = nowMs - profileState_.phaseStartMs;
    if (elapsed >= profileState_.phaseDurationMs) return 0;
    return profileState_.phaseDurationMs - elapsed;
  }
  uint16_t getProfileTargetTemp() const {
    if (profileState_.currentStage >= profileData_.stageCount) return 0;
    return profileData_.stages[profileState_.currentStage].holdTempC;
  }
  void startPidAutoTune(uint16_t targetTemp, uint8_t cycles, AutoTuneTarget which = AutoTuneTarget::HeaterTemp, float bandC = 2.0f, float power01 = 1.0f);
  void tickPidAutoTune(uint32_t nowMs);
  void getPidAutoView(PidAutoView &out, uint32_t nowMs) const;
  float heaterPower01() const { return lastDuty01_; }
  bool fanOn() const { return fan_on_; }
  DryerInputs getInputs() const { return inputs_; }
  void stop();

  void emergencyFanOn(bool state = true) { driveFan(state); }
  void emergencyStop(bool state = true);

  uint32_t dryStartMs() const { return dryStartMs_; }
  uint32_t dryTotalMs() const { return dryTotalMs_; }

  DryerMode mode() const { return mode_; }
  void setMode(DryerMode mode) { mode_ = mode; }

  // ЕДИНАЯ точка переключения режима
  void requestMode(DryerMode m, uint16_t tempC = 0, uint16_t minutes = 0, uint8_t hum = 0, bool storageByHum = true);

  void setFlapOpenBool(bool open) { flap_is_open_ = open; }
  void setFlapOpen(bool open);
  void setFlapLevel(float level01);
  void updateServoSmooth();
  void applyServoFromMenu(uint8_t idx, bool apply_now);
  void updateServoTimers_(uint32_t nowMs, uint32_t openTimeMs, uint32_t closeTimeMs);
  void setServoTarget_(int angle, bool immediate = false);
  static inline int clampAngle_(int a) { return a < 0 ? 0 : (a > 180 ? 180 : a); }

  void previewServoAngle(uint8_t angle);
  // void updateFlapByHumidity(float rh, float dRh);

  // RH servo
  RhRing rhRing_;
  TrendCfg trendCfg_;
  TrendPhase prevPhase_ = TrendPhase::Unknown;
  uint32_t stateSinceMs_ = 0;
  void rhPush(float rh, uint32_t nowMs);
  RhTrend rhTrendLinear(int N) const;
  TrendPhase decideByTrend(const RhTrend &tr) const;
  void updateDamperByTrend(uint32_t nowMs);
  const char *phaseToStr(TrendPhase p);

private:
  uint32_t getProfileCompletedPlannedSeconds_() const {
    uint32_t sum = 0;
    uint8_t limit = profileState_.currentStage;
    if (limit > profileData_.stageCount) limit = profileData_.stageCount;
    for (uint8_t i = 0; i < limit; ++i) {
      sum += profileData_.stages[i].rampSeconds;
      sum += profileData_.stages[i].holdSeconds;
    }
    return sum;
  }
  uint32_t getProfilePlannedTotalSeconds_() const {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < profileData_.stageCount; ++i) {
      sum += profileData_.stages[i].rampSeconds;
      sum += profileData_.stages[i].holdSeconds;
    }
    return sum;
  }
  void driveHeater(float power01);
  void driveFan(bool on);
  void updateFanSimple(float tempC);
  bool timeExpired_(uint32_t startMs, uint32_t totalMs) const;

  // float computeHumidityTrend(float rh_now);
  // float computeHumidityTrend();
  // void updateFlapByTrend(float rh, float dRh, uint32_t nowMs);
  // void addRhPoint(uint8_t rh);
  // void updateRhHistory(unsigned long now, uint8_t rh_now);

  static inline float clampf(float v, float lo, float hi);
  float computeHeaterTargetFromAir(float air_temp, float heater_temp, float air_target, float deltaC, float nowSec);
  float runHeaterInnerPID_ToDuty01(float heater_temp, float heater_target, float nowSec, float heater_abs_maxC);
  void storageControlByHumidity(uint32_t nowMs, float air_hum, uint8_t idx);
  float storageTargetByTemperature(uint8_t idx);

  // static void dumpPid(const char* name, const PID_Controller* p);
  void dumpPid(const PID_Controller *p);
  void updatePid();

  /**  ---- Verify Heater (klipper-like) ---- */
  struct VerifyHeaterState {
    bool active = false;      // был хотя бы один валидный вызов
    bool armed = false;       // проверка включена (идёт нагрев)
    bool approaching = false; // идём по лестнице
    float err = 0.0f;         /// интеграл недогрева
    uint32_t lastTickMs = 0;  // 1 Гц тик для интеграла
    // Лестница
    float goalTemp = 0.0f;       // текущая ступень: T_now + VH_HEATING_GAIN
    uint32_t goalDeadlineMs = 0; // дедлайн на взятие ступени
    // Snapshot внутренней цели (замороженная цель для проверки)
    float snapshotTarget = NAN;     // именно по ней считаем inBand/дефицит
    float snapshotAtSetpoint = NAN; // какой setpoint был в момент съёма
  } vh_;


  bool verifyHeaterUpdate_(uint32_t nowMs, float heater_temp, float heater_setpoint, float duty01, uint8_t idx);
  void resetVerifyHeater_(uint32_t nowMs, float heater_temp);

  /**  ---- Verify Heater (klipper-like) ---- */

  DryerMode mode_ = DryerMode::Idle;

  uint8_t unitIndex_ = 0;
  int16_t targetTemp_ = 0;
  uint8_t targetHum_ = 0;
  int16_t lastDryTemp_ = 0;
  int16_t storageTemp_ = 0;

  uint32_t dryStartMs_ = 0;
  uint32_t dryModeStartMs_ = 0;
  uint32_t dryTotalMs_ = 0;
  static constexpr float TARGET_BAND_C = 1.0f; // допуска вокруг target для выхода в цель

  bool storageHeating_ = false;      // греем сейчас в режиме Storage по RH?
  uint32_t storageLastToggleMs_ = 0; // антидребезг переключений
  bool needHeatingBoost_ = false;
  uint32_t lastStorageOnMs_ = 0;
  uint32_t lastDryLog = 0;

  // --- Cascaded PID controllers (outer: air -> heater setpoint, inner: heater -> PWM) ---
  PID_Controller pid_air_;
  PID_Controller pid_heater_;
  uint32_t pidUpdateIntervalMs_ = PID_UPDATE_INTERVAL_MS; // compute every 250 ms
  uint32_t pidPrevMs_ = 0;

  float lastDuty01_ = 0.0f; // обновляем в driveHeater()
  PidAutoState auto_;       // состояние автотюна
  bool autoApplied_ = false;

  // Profile mode state
  ProfileData profileData_;
  ProfileState profileState_;

  // telemetry/state for cascade
  float air_target_ = 0.0f;
  float heater_setpoint_ = 0.0f; // target heater temp produced by air PID
  float pwm_percent_ = 0.0f;     // 0..100 from heater PID

  DryerInputs inputs_;

  // Пины
  uint8_t heaterPin_;
  uint8_t fanPin_;
  uint8_t servoPin_;
  uint16_t pwmRange_;
  uint32_t pwmFreq_;
  // fan state
  bool fan_on_ = false;
  // Углы сервы (по умолчанию)
  uint8_t angleClosed_ = 40;
  uint8_t angleOpen_ = 60;

  int servoCurrentAngle_ = 0; // где реально стоим
  int servoTargetAngle_ = 0;  // куда хотим прийти
  uint32_t lastServoStepMs_ = 0;

  Servo flapServo_;
  bool flap_is_open_ = false;   // состояние заслонки
  bool flap_want_open_ = false; // состояние заслонки
  uint32_t lastServoMs = 0;

  uint32_t lastPidLogMs_ = 0; //! test
  uint32_t lastTrendMs_ = 0;  // таймер пересчёта тренда RH
  float sinInc = 0;
  uint32_t everyTick = 0;

  // PID tuning ON/OFF
  struct HeaterPidBackup {
    bool valid = false;
    float deriv_alpha;
    float slew_per_sec;
    float out_min, out_max;
    float min_on, off_thresh;
    float taper_band;
    bool fan_on;
    bool flap_is_open;
  };

  void enterAutoTuneMode_(); // выключаем улучшайзеры, фиксируем механику
  void leaveAutoTuneMode_(); // возвращаем всё как было

  HeaterPidBackup bak_;

#ifdef USE_SOFT_PWM
  int pwmChannel_ = -1;
#endif
};

extern DryerController gDryer;

extern HX711Multi *hx711MultiPtr;
#define hx711Multi (*hx711MultiPtr)
