#include "control.h"
#include "menu/menu_eeprom.h"    // Для EE_OFF_*_SESSION_COUNT констант
#include "menu/menu_eeprom_io.h" // Для ee_read, ee_store_field
#include "menu/menu_state.h"
#include "session/session_counters.h"
#include <math.h>

#include "error/error_bus.h"
#include "error/error_post.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_CONTROL
#define LOG_TAG "CTRL"
#include "debug_log.h"


#if defined(ARDUINO_ARCH_RP2040)
extern void analogWriteFreq(uint32_t freq);
extern void analogWriteRange(uint32_t range);
#endif

extern "C" {
void pid_recalc_heater(void *u);
void pid_recalc_chamber(void *u);
}

void DryerController::getPidAutoView(PidAutoView &o, uint32_t nowMs) const {
  o.active = (mode_ == DryerMode::PidAutoTune) && auto_.active;
  o.heating = auto_.heating;
  o.targetSel = auto_.targetSel;
  o.targetC = auto_.targetC;
  o.bandC = auto_.bandC;
  o.peaks = auto_.peaks;
  o.needCycles = auto_.needCycles;

  o.currentTempC = (auto_.targetSel == AutoTuneTarget::HeaterTemp) ? inputs_.heaterTempC : inputs_.airTempC;
  o.duty01 = lastDuty01_;
  o.sinceToggleMs = nowMs - auto_.lastToggleMs;

  o.resultOk = auto_.result.ok;
  o.Kp = auto_.result.Kp;
  o.Ki = auto_.result.Ki;
  o.Kd = auto_.result.Kd;
}

extern MenuState menu;
// DryerController iDryer(HEATER, FAN, SERVO, 255, 5); // частота 5 Гц

DryerController::DryerController(uint8_t heaterPin, uint8_t fanPin, uint8_t servoPin, uint16_t heaterPwmRange, uint32_t heaterPwmFreqHz, uint8_t unitIndex) : heaterPin_(heaterPin), fanPin_(fanPin), servoPin_(servoPin), pwmRange_(heaterPwmRange), pwmFreq_(heaterPwmFreqHz), unitIndex_(unitIndex) {}

void DryerController::begin() {
  // Серво
  flapServo_.attach(servoPin_);
  // стартовая позиция: ЗАКРЫТО (один write, без дёрганья)
  servoCurrentAngle_ = angleClosed_;
  servoTargetAngle_ = angleClosed_;
  flapServo_.write(servoCurrentAngle_);
  flap_is_open_ = false;
  lastServoMs = millis();
  lastServoStepMs_ = lastServoMs;

  DEBUG_I("[%d] Servo is closed angle: %d", index(), angleClosed_);

  // Вентилятор
  pinMode(fanPin_, OUTPUT);
  digitalWrite(fanPin_, LOW);
  fan_on_ = false;

  // Нагреватель
  // Нагреватель
#ifndef USE_SOFT_PWM
#if defined(ARDUINO_ARCH_RP2040)
  analogWriteFreq(pwmFreq_);   // частота ТОЛЬКО для пина нагревателя (его слайс)
  analogWriteRange(pwmRange_); // диапазон счётчика
#endif
  pinMode(heaterPin_, OUTPUT);
  analogWrite(heaterPin_, 0); // старт 0% duty
#else
  // Софт‑PWM: не трогаем analogWrite*/частоты вовсе
  pinMode(heaterPin_, OUTPUT);
  pwmChannel_ = SoftPwmManager::instance().addChannel(heaterPin_, (float)pwmFreq_, 0.0f);
#endif
}

void DryerController::setAirPid(uint8_t controller, PID_Actuator kind, float kp, float ki, float kd, float minOut, float maxOut, float sampleSec, float gain_multiplier) {
  // Air: d_T=0.2..0.4, deriv_alpha=0.3..0.6, slew_per_sec=0, окно без min_on/off, taper_band=0.8..1.2.

  PID_Init(&pid_air_, controller, kind, kp, ki, kd,
           /*out_min=*/minOut,
           /*out_max=*/maxOut,
           /*min_dt=*/sampleSec,
           /*gain_multiplier=*/gain_multiplier);

  dumpPid(&pid_air_);
  // PID_SetDerivFilterAlpha(&pid_air_, 0.3f);
  // PID_SetTaperBand(&pid_air_, 0.5f);
}

void DryerController::updateHeaterGain(float gain_multiplier) { PID_UpdateGain(&pid_heater_, gain_multiplier, pid_heater_.Kp); }

void DryerController::updateAirGain(float gain_multiplier) { PID_UpdateGain(&pid_air_, gain_multiplier, pid_air_.Kp); }

void DryerController::setHeaterPid(uint8_t controller, PID_Actuator kind, float kp, float ki, float kd, float minOut, float maxOut, float sampleSec, float gain_multiplier) {
  PID_Init(&pid_heater_, controller, kind, kp, ki, kd,
           /*out_min=*/minOut,
           /*out_max=*/maxOut,
           /*min_dt=*/sampleSec,
           /*setHeaterPid=*/gain_multiplier);

  dumpPid(&pid_heater_);

  // PID_SetDerivFilterTime (&pid_heater_, 0.80f);     // d_T, с
  // PID_SetDerivFilterAlpha(&pid_heater_, 0.85f);     // EMA альфа для D
  // PID_SetSlew            (&pid_heater_, 25.0f);     // %/с
  // PID_SetOutputWindow    (&pid_heater_, 0.0f, 100.0f,
  //                         /*min_on*/  20.0f,
  //                         /*off_thr*/ 18.0f);

  // PID_SetTaperBand(&pid_heater_, 1.8f);

  // DEBUG_I("[PID][HEATER][improv] dT=%.2f alpha=%.2f slew=%.1f min_on=%.1f off=%.1f taper=%.2f",
  //         (double)pid_heater_.d_T,
  //         (double)pid_heater_.deriv_alpha,
  //         (double)pid_heater_.slew_per_sec,
  //         (double)pid_heater_.min_on,
  //         (double)pid_heater_.off_thresh,
  //         (double)pid_heater_.taper_band);
}

void DryerController::enterAutoTuneMode_() {
  // сохранить текущее состояние PID-настройки (улучшайзеры)
  // bak_.deriv_alpha  = pid_heater_.deriv_alpha;
  // bak_.slew_per_sec = pid_heater_.slew_per_sec;
  // bak_.out_min      = pid_heater_.output_min;
  // bak_.out_max      = pid_heater_.output_max;
  // bak_.min_on       = pid_heater_.min_on;
  // bak_.off_thresh   = pid_heater_.off_thresh;
  // bak_.taper_band   = pid_heater_.taper_band;

  // сохранить механику/обдув
  bak_.fan_on = fan_on_;
  bak_.flap_is_open = flap_is_open_;
  bak_.valid = true;

  // --- отключить все улучшайзеры для ЧИСТОГО релейного сигнала 0/100 ---
  // PID_SetDerivFilterAlpha(&pid_heater_, 1.0f);  // без фильтра
  // PID_SetSlew(&pid_heater_, 0.0f);              // без ограничения скорости
  // PID_SetTaperBand(&pid_heater_, 0.0f);         // без ослабления у цели
  // PID_SetOutputWindow(&pid_heater_,
  //                     /*out_min*/ 0.0f,
  //                     /*out_max*/ 100.0f,
  //                     /*min_on */ 0.0f,
  //                     /*off_thr*/ 0.0f);        // без порогов окна

  // зафиксировать чтобы динамика объекта не менялась
  driveFan(bak_.fan_on); //  вручную true/false
  // setFlapOpen(bak_.flap_is_open); // или зафиксируем в нужном положении
  setServoTarget_(menu.servo_open_angle[index()], true);
}

void DryerController::leaveAutoTuneMode_() {
  if (!bak_.valid) return;

  driveFan(bak_.fan_on);
  setFlapOpen(bak_.flap_is_open);

  bak_.valid = false;
}

void DryerController::dumpPid(const PID_Controller *p) {
  const float gm = p->gain_multiplier;
  const float Kp_eff = p->Kp * gm;
  const float Ki_eff = p->Ki * gm;
  const float Kd_eff = p->Kd * gm;

  DEBUG_T("[PID][id=%u kind=%s]\n"
          "Base:      Kp=%.3f  Ki=%.3f  Kd=%.3f\n"
          "Gain:      gm=%.3f  (P_level≈%.3f per °C)\n"
          "Effective: Kp=%.3f  Ki=%.3f  Kd=%.3f\n"
          "Out:       [%.1f .. %.1f]\n"
          "dt_min:    %.2fs\n"
          "Gain_mult: %.2f",
          (unsigned)p->id, pid_actuator_str(p->kind), (double)p->Kp, (double)p->Ki, (double)p->Kd, (double)gm, (double)Kp_eff, (double)Kp_eff, (double)Ki_eff, (double)Kd_eff, (double)p->output_min, (double)p->output_max, (double)p->min_dt, (double)p->gain_multiplier);
}

void DryerController::resetPid() {
  PID_ResetState(&pid_air_);
  PID_ResetState(&pid_heater_);
  pidPrevMs_ = millis();
  dumpPid(&pid_air_);
  dumpPid(&pid_heater_);
}

void DryerController::updatePid() {
  uint8_t u = index();
  setAirPid(u, PID_ACT_AIR, menu.pid_kp_chamber[u], menu.pid_ki_chamber[u], menu.pid_kd_chamber[u],
            /*minOut*/ 0.0f,
            /*maxOut*/ menu.heater_max_temp[u], // menu.air_max_temp[u],
            /*sample*/ PID_UPDATE_INTERVAL_S,
            /*gain_multiplier*/ menu.pid_gain_chamber[u]);
  setHeaterPid(u, PID_ACT_HEATER, menu.pid_kp_heater[u], menu.pid_ki_heater[u], menu.pid_kd_heater[u],
               /*minOut*/ 0.0f,
               /*maxOut*/ 100.0f,
               /*sample*/ PID_UPDATE_INTERVAL_S,
               /*gain_multiplier*/ menu.pid_gain_heater[u]);
}

void DryerController::setServoLimits(uint8_t angleClosed, uint8_t angleOpen) {
  // Пользователь задаёт углы руками
  angleClosed_ = angleClosed;
  angleOpen_ = angleOpen;
}

void DryerController::startDrying(uint16_t tempC, uint16_t minutes) {
  // ✅ Инкремент счетчика сессий Drying только при смене режима
  // Если режим уже Drying, значит это обновление параметров - инкремент не нужен
  // sessionNum = dryingCount + storageCount + profileCount
  if (mode_ != DryerMode::Drying) {
    const uint16_t dryingCount = sessionCountersIncrement(menu, SessionCounterKind::Drying);
    DEBUG_I("[SESSION][%d] Drying session count incremented: %u", index(), dryingCount);
  }

  mode_ = DryerMode::Drying;
  targetTemp_ = tempC;
  dryModeStartMs_ = millis();
  dryStartMs_ = 0;
  // dryStartMs_ = millis();
  dryTotalMs_ = (uint32_t)minutes * 60UL * 1000UL;
  lastDryTemp_ = (float)tempC;
  // driveHeater(true);
  driveFan(true);
  setFlapOpen(false);
  DEBUG_I("[SERVO_CMD][%d] startDrying -> OPEN (immediate=0)", index());
  POST_ERROR(ERRSEV_INFO, index(), ERRSRC_DRYING, ERRC_STATE_CHANGE, "Mode=DRYING start", (int32_t)targetTemp_);
  updatePid(); //! TEST
  resetPid();
}

void DryerController::startStorage(uint16_t tempC, uint8_t targetHum, bool storageByHum) {
  // ✅ Инкремент счетчика сессий Storage только при смене режима
  // Если режим уже Storage, значит это обновление параметров - инкремент не нужен
  // sessionNum = dryingCount + storageCount + profileCount
  if (mode_ != DryerMode::Storage) {
    const uint16_t storageCount = sessionCountersIncrement(menu, SessionCounterKind::Storage);
    DEBUG_I("[SESSION][%d] Storage session count incremented: %u", index(), storageCount);
  }

  mode_ = DryerMode::Storage;
  targetTemp_ = tempC;
  targetHum_ = targetHum;
  dryModeStartMs_ = 0;
  dryStartMs_ = 0;
  dryTotalMs_ = 0;
  // driveHeater(true);
  driveFan(true);
  setFlapOpen(false);
  DEBUG_I("[SERVO_CMD][%d] startStorage -> OPEN (immediate=0)", index());
  POST_ERROR(ERRSEV_INFO, index(), ERRSRC_STORAGE, ERRC_STATE_CHANGE, "Mode=STORAGE start", (int32_t)targetTemp_);
  updatePid();
  resetPid();
}

void DryerController::startProfile(const ProfileData &prof, uint8_t startFromStage) {
  // ✅ Инкремент счетчика сессий Profile только при смене режима
  // Profile всегда запускается как новая сессия, но проверяем для единообразия
  // sessionNum = dryingCount + storageCount + profileCount
  if (mode_ != DryerMode::Profile) {
    const uint16_t profileCount = sessionCountersIncrement(menu, SessionCounterKind::Profile);
    DEBUG_I("[SESSION][%d] Profile session count incremented: %u", index(), profileCount);
  }

  mode_ = DryerMode::Profile;
  profileData_ = prof;

  // Инициализируем состояние профиля - начинаем с RAMP (выход на температуру)
  profileState_.currentStage = startFromStage;
  profileState_.phase = ProfileState::Phase::Ramp;
  profileState_.phaseStartMs = millis();
  profileState_.rampStartTempC = inputs_.airTempC; // Запоминаем начальную температуру для RAMP

  // Устанавливаем время и целевую температуру для RAMP фазы
  if (startFromStage < prof.stageCount) {
    targetTemp_ = prof.stages[startFromStage].holdTempC;
    profileState_.phaseDurationMs = (uint32_t)prof.stages[startFromStage].rampSeconds * 1000UL;
  }

  driveFan(true);
  setFlapOpen(false);
  DEBUG_I("[PROFILE][%d] startProfile stageCount=%u startFrom=%u temp=%uC rampSec=%u", index(), prof.stageCount, startFromStage, startFromStage < prof.stageCount ? prof.stages[startFromStage].holdTempC : 0, startFromStage < prof.stageCount ? prof.stages[startFromStage].rampSeconds : 0);
  POST_ERROR(ERRSEV_INFO, index(), ERRSRC_DRYING, ERRC_STATE_CHANGE, "Mode=PROFILE start", (int32_t)startFromStage);
  updatePid();
  resetPid();
}

void DryerController::tickProfile(uint32_t nowMs) {
  uint8_t idx = index();
  const float air_temp = inputs_.airTempC;

  // Проверка границ текущего стейджа
  if (profileState_.currentStage >= profileData_.stageCount) {
    DEBUG_W("[PROFILE][%d] currentStage=%u out of bounds (count=%u), stop", idx, profileState_.currentStage, profileData_.stageCount);
    stop();
    return;
  }

  uint32_t elapsedMs = nowMs - profileState_.phaseStartMs;
  ProfileStage &currStage = profileData_.stages[profileState_.currentStage];

  if (profileState_.phase == ProfileState::Phase::Ramp) {
    // ===== RAMP ФАЗА: выход на температуру стейджа =====
    bool rampFinished = (elapsedMs >= profileState_.phaseDurationMs);

    if (!rampFinished && profileState_.phaseDurationMs > 0) {
      // Вычисляем плавную микро-цель по времени (от rampStartTempC к holdTempC)
      float progress = (float)elapsedMs / (float)profileState_.phaseDurationMs;
      float microTarget = profileState_.rampStartTempC + (currStage.holdTempC - profileState_.rampStartTempC) * progress;
      targetTemp_ = (uint16_t)lroundf(microTarget);

      DEBUG_T("[PROFILE][%d] RAMP: %.1f°C → %.0f°C (%.0f%%, target=%.1f°C)", idx, (double)profileState_.rampStartTempC, (double)currStage.holdTempC, (double)(progress * 100.0f), (double)microTarget);
    } else {
      // RAMP завершен → переход в HOLD
      targetTemp_ = currStage.holdTempC;
      profileState_.phase = ProfileState::Phase::Hold;
      profileState_.phaseStartMs = nowMs;
      profileState_.phaseDurationMs = (uint32_t)currStage.holdSeconds * 1000UL;

      DEBUG_I("[PROFILE][%d] RAMP finished stage %u → HOLD (target=%.0f°C, %u sec)", idx, profileState_.currentStage, (double)currStage.holdTempC, currStage.holdSeconds);
    }
  } else if (profileState_.phase == ProfileState::Phase::Hold) {
    // ===== HOLD ФАЗА: удержание температуры стейджа =====
    targetTemp_ = currStage.holdTempC; // Держим целевую температуру

    const float TARGET_BAND = 1.0f;
    bool tempReached = (air_temp >= (currStage.holdTempC - TARGET_BAND));
    bool timeExpired = (elapsedMs >= profileState_.phaseDurationMs);

    DEBUG_T("[PROFILE][%d] HOLD: temp=%.1f°C target=%.0f°C reached=%d time=%u/%u", idx, (double)air_temp, (double)currStage.holdTempC, tempReached, elapsedMs / 1000, profileState_.phaseDurationMs / 1000);

    if (tempReached && timeExpired) {
      // HOLD завершен → переход к следующему стейджу или конец
      uint8_t nextStage = profileState_.currentStage + 1;

      if (nextStage < profileData_.stageCount) {
        // Есть следующий стейдж → начинаем RAMP к нему
        profileState_.currentStage = nextStage;
        profileState_.phase = ProfileState::Phase::Ramp;
        profileState_.phaseStartMs = nowMs;
        profileState_.rampStartTempC = air_temp; // От текущей температуры
        profileState_.phaseDurationMs = (uint32_t)profileData_.stages[nextStage].rampSeconds * 1000UL;

        DEBUG_I("[PROFILE][%d] HOLD finished stage %u → RAMP to stage %u (%.0f→%.0f°C, %u sec)", idx, profileState_.currentStage - 1, nextStage, (double)air_temp, (double)profileData_.stages[nextStage].holdTempC, profileData_.stages[nextStage].rampSeconds);

        // Уведомление о переходе к следующему стейджу
        POST_ERROR(ERRSEV_INFO, idx, ERRSRC_DRYING, ERRC_STATE_CHANGE, "Profile=NEXT_STAGE", (int32_t)nextStage);

        // Пишем текущий стейдж в EEPROM
        ee_store_field(EE_OFF_START_STAGE_BY_NUMBER(idx), profileState_.currentStage + 1);
      } else {
        // Профиль завершен
        DEBUG_I("[PROFILE][%d] Profile finished at stage %u", idx, profileState_.currentStage);
        ee_store_field(EE_OFF_START_STAGE_BY_NUMBER(idx), 0);
        stop();
        POST_ERROR(ERRSEV_INFO, idx, ERRSRC_DRYING, ERRC_STATE_CHANGE, "Profile=DONE", 0);
        return;
      }
    }
  }
}

void DryerController::stop() {
  mode_ = DryerMode::Idle;
  dryModeStartMs_ = 0;
  dryStartMs_ = 0;
  dryTotalMs_ = 0;
  driveHeater(0.0f);
  driveFan(false);
  setFlapOpen(false);
  resetVerifyHeater_(millis(), inputs_.airTempC);
  DEBUG_T("[SERVO_CMD][%d] stop -> CLOSE (immediate=0)", index());
  DEBUG_T("DryerController::stop()[%d]", index());
  if (!post_err_ex(index(), ERRSRC_MODE, ERRSEV_INFO, ERRC_STATE_CHANGE, "Mode=IDLE", 0, millis())) {
    DEBUG_W("[ERRBUS] in stop() drop: Mode=IDLE");
  }
}

void DryerController::emergencyStop(bool state) {
  fan_on_ = true;
  driveHeater(0.0f);
  driveFan(fan_on_);
  setFlapOpen(true);
  resetVerifyHeater_(millis(), inputs_.heaterTempC);
  DEBUG_T("[SERVO_CMD][%d] stop -> OPEN (immediate=0)", index());
  DEBUG_T("DryerController::stop()[%d]", index());
}

bool DryerController::timeExpired_(uint32_t startMs, uint32_t totalMs) const { return (millis() - startMs) >= totalMs; }

// TODO:
void DryerController::requestMode(DryerMode m, uint16_t tempC, uint16_t minutes, uint8_t hum, bool storageByHum) {
  if (mode_ == m) {
    // уже в нужном режиме - можно просто обновить параметры при необходимости
    if (m == DryerMode::Drying) {
      startDrying(tempC, minutes); // переустановит цели
    } else if (m == DryerMode::Storage) {
      startStorage(tempC, hum, storageByHum);
    }
    return;
  }

  stop();

  switch (m) {
  case DryerMode::Idle:
    break;
  case DryerMode::Drying:
    startDrying(tempC, minutes);
    break;
  case DryerMode::Storage:
    startStorage(tempC, hum, storageByHum);
    break;
  case DryerMode::Profile:
    // Для Profile используется startProfile() напрямую из меню
    // Здесь обработка не требуется
    break;
  case DryerMode::PidAutoTune:
    startPidAutoTune(/*targetTemp=*/tempC, /*cycles=*/5);
    break;
  }
}

void DryerController::tick() {

  const uint32_t nowMs = millis();

  if (mode_ == DryerMode::Error) {
    // в аварийном режиме - только вентилятор включен, всё остальное отключено
    emergencyStop(true);
    return;
  }

  /** -------  AUTOPID TUNE  -------------*/
  if (mode_ == DryerMode::PidAutoTune) {
    driveFan(true);
    tickPidAutoTune(nowMs);

    // если уже всё применили и идёт показ результата - уйти в Idle по таймеру
    if (autoApplied_ && !auto_.active) {
      if ((int32_t)(nowMs - auto_.resultShowUntilMs) >= 0) {
        autoApplied_ = false;
        mode_ = DryerMode::Idle;
        DEBUG_T("[PID][AUTO][%d] result show time expired -> back to IDLE", index());
      }
      return;
    }

    // Финиш автотюна → применить и сохранить
    if (!autoApplied_ && !auto_.active) {
      const uint8_t u = index();

      if (auto_.result.ok) {
        if (auto_.targetSel == AutoTuneTarget::HeaterTemp) {
          menu.pid_kp_heater[u] = auto_.result.Kp;
          menu.pid_ki_heater[u] = auto_.result.Ki;
          menu.pid_kd_heater[u] = auto_.result.Kd;
          menu.pid_gain_heater[u] = auto_.result.Kp;
          pid_recalc_heater((void *)(uintptr_t)u);
        } else {
          menu.pid_kp_chamber[u] = auto_.result.Kp;
          menu.pid_ki_chamber[u] = auto_.result.Ki;
          menu.pid_kd_chamber[u] = auto_.result.Kd;
          menu.pid_gain_chamber[u] = auto_.result.Kp;
          pid_recalc_chamber((void *)(uintptr_t)u);
        }
        menu.saveToEEPROM();
        DEBUG_I("[PID][AUTO] applied & saved U%u  Kp=%.3f Ki=%.3f Kd=%.3f", u, auto_.result.Kp, auto_.result.Ki, auto_.result.Kd);
      } else {
        DEBUG_W("[PID][AUTO][%d] finished without result (aborted?) – skip apply", index());
      }
      leaveAutoTuneMode_();
      autoApplied_ = true;
      driveHeater(0.0f);
      setFlapOpen(false);
      // mode_ = DryerMode::Idle;
    }
    return;
  }

  /** -------  PROFILE MODE  ----------- */
  if (mode_ == DryerMode::Profile) {
    tickProfile(nowMs);
    // После tickProfile мы либо остаемся в Profile, либо переходим в Idle (в tickProfile)
    // Основная логика PID выполнится ниже
    if (mode_ != DryerMode::Profile) {
      return; // Профиль завершен, stop() уже вызван
    }
  }

  const bool isDry = (mode_ == DryerMode::Drying);
  const bool isStore = (mode_ == DryerMode::Storage);
  const bool isProfile = (mode_ == DryerMode::Profile);
  const float air_temp = inputs_.airTempC;
  const float heater_temp = inputs_.heaterTempC;
  const float air_hum = inputs_.airHumRH;
  uint8_t idx = index();

  /** ===  EMERGENCY STOPS when the temperature is exceeded === */
  if (air_temp >= menu.air_max_temp[idx] + 5.0f) {
    if (isDry || isStore || isProfile) {
      emergencyStop(true);
      POST_ERROR(ERRSEV_CRITICAL, idx, ERRSRC_AIR, ERRC_OVER_MAX, "Air over max", (int32_t)lroundf(air_temp));
      return;
    }
  }
  if (heater_temp >= menu.heater_max_temp[idx] + 10.0f) {
    if (isDry || isStore || isProfile) {
      emergencyStop(true);
      DEBUG_E("[STOP][%d] overheat heaterTemp=%.1fC >= max=%.1fC", idx, (double)heater_temp, (double)menu.heater_max_temp[idx]);
      POST_ERROR(ERRSEV_CRITICAL, idx, ERRSRC_HEATER, ERRC_OVER_MAX, "Heater over max", (int32_t)lroundf(heater_temp));
      return;
    }
  }
  if (isDry || isStore || isProfile) {
  }

  if (!isStore) updateFanSimple(inputs_.heaterTempC);

  float air_target = NAN;
  uint32_t openTimeMs = (uint32_t)menu.servo_time_open[idx] * 1000UL;
  uint32_t closeTimeMs = (uint32_t)menu.servo_time_closed[idx] * 1000UL;

  // switching from drying to storage with parameter transfer
  if (isDry && dryStartMs_ != 0 && timeExpired_(dryStartMs_, dryTotalMs_)) {
    DEBUG_E("to Storage");
    uint32_t elapsed = nowMs - dryStartMs_;
    int32_t remain = (int32_t)dryTotalMs_ - (int32_t)elapsed;

    DEBUG_T("[CHK][%u] drying done edge: now=%lu start=%lu total=%lu elapsed=%lu remain=%ld", (unsigned long)nowMs, (unsigned long)dryStartMs_, (unsigned long)dryTotalMs_, (unsigned long)elapsed, (long)remain);

    DEBUG_T("[CHK][%d] storage_auto=%d hum_prio=%d storage_temp=%u dry_temp=%.1f pct=%u", idx, (int)menu.storage_auto[idx], (int)menu.storage_hum_priority[idx], (unsigned)menu.storage_temp[idx], (double)menu.dry_temp[idx], (unsigned)menu.storage_dry_temp_by_dry_temp[idx]);

    if (menu.storage_auto[idx]) {
      // 1) Целевая температура хранения
      // uint16_t storageTempC =
      //         menu.storage_dry_temp_mode[idx]
      //         // ? (uint16_t)lroundf( menu.dry_temp[idx] * (menu.storage_dry_temp_by_dry_temp[idx] / 100.0f))
      //         ? (uint16_t)lroundf( targetTemp_ * (menu.storage_dry_temp_by_dry_temp[idx] / 100.0f))
      //         : (uint16_t)menu.storage_temp[idx];

      // const float lo = 25.0f;
      // const float hi = menu.air_max_temp[idx];
      // if (storageTempC < lo) storageTempC = lo;
      // if (storageTempC > hi) storageTempC = hi;

      storageTemp_ = storageTargetByTemperature(idx);

      // 2) Целевая влажность и режим приоритета
      uint8_t targetHum = (uint8_t)menu.storage_hum[idx];
      bool storageByHum = menu.storage_hum_priority[idx];

      DEBUG_I("[MODE][%d] Drying finished -> startStorage(T=%uC, RH=%u%%, byHum=%d)", index(), storageTemp_, targetHum, storageByHum);

      // Уведомление о завершении сушки перед переходом в Storage
      POST_ERROR(ERRSEV_INFO, idx, ERRSRC_DRYING, ERRC_STATE_CHANGE, "Drying=DONE", 0);

      dryStartMs_ = 0;
      dryTotalMs_ = 0;

      startStorage(storageTemp_, targetHum, storageByHum);
      return;
    } else {
      // Уведомление о завершении сушки без перехода в Storage
      POST_ERROR(ERRSEV_INFO, idx, ERRSRC_DRYING, ERRC_STATE_CHANGE, "Drying=DONE", 0);
      stop();
      DEBUG_I("[STOP DRYING][%d] STOP DRYING", index());
      return;
    }
  }

  updateFanSimple(heater_temp);
  rhPush(air_hum, nowMs); // раз в RH_SAVE_PERIOD_MS

  // complete tick if not drying or storage
  if (!(isDry || isStore || isProfile)) {
    setFlapOpen(false); // TODO: Проверить
    updateServoSmooth();
    return;
  }

  // === SMART SERVO CONTROL ===
  // float dRh = computeHumidityTrend(); // вычисляем тренд
  const bool smart_servo = menu.servo_smart_mode[idx];
  if (smart_servo && dryStartMs_) {
    updateDamperByTrend(nowMs);
  } else if (dryStartMs_) {
    // setFlapOpen(flap_is_open_);
    updateServoTimers_(nowMs, openTimeMs, closeTimeMs);
    // Страховка: интерпретация нулевых таймеров
    //  - openTimeMs == 0  → держим ЗАКРЫТО (если close>0)
    //  - closeTimeMs == 0 → держим ОТКРЫТО (если open>0)
    if (openTimeMs == 0 && closeTimeMs > 0) {
      if (flap_is_open_) {
        DEBUG_T("[SERVO_CMD][%d] TIMER force CLOSE (openTime=0)", index());
        setFlapOpen(flap_is_open_ = false);
      }
    } else if (closeTimeMs == 0 && openTimeMs > 0) {
      if (!flap_is_open_) {
        DEBUG_T("[SERVO_CMD][%u] TIMER force OPEN (closeTime=0)", index());
        setFlapOpen(flap_is_open_ = true);
      }
    } else {
      // В один тик допускаем только один переход
      if (flap_is_open_) {
        if (openTimeMs > 0 && (nowMs - lastServoMs) >= openTimeMs) {
          DEBUG_T("[SERVO_CMD][%u] TIMER close (immediate=0)", index());
          setFlapOpen(flap_is_open_ = false);
        }
      } else { // закрыто
        if (closeTimeMs > 0 && (nowMs - lastServoMs) >= closeTimeMs) {
          DEBUG_T("[SERVO_CMD][%d] TIMER open (immediate=0)", index());
          setFlapOpen(flap_is_open_ = true);
        }
      }
    }
  }

  // === HEATER CASCADED PID CONTROL BY MODE ===
  if (mode_ == DryerMode::Drying) {
    air_target = targetTemp_;
  } else if (mode_ == DryerMode::Profile) {
    // Profile: targetTemp_ уже установлена в tickProfile с учетом HOLD/RAMP
    air_target = targetTemp_;
    DEBUG_T("[PROFILE][%d] PID: target=%.0f°C current=%.1f°C", idx, (double)air_target, (double)air_temp);
  } else if (mode_ == DryerMode::Storage) {
    if (menu.storage_hum_priority[idx]) {
      storageControlByHumidity(nowMs, air_hum, idx);
      if (storageHeating_) {
        air_target = storageTargetByTemperature(idx);
        updateFanSimple(inputs_.heaterTempC);
        DEBUG_I("[MODE][%d] Storage: control=HUM, heating=1, "
                "(storageTemp=%.1fC RH=%.1f%% targetRH=%u%% hystRH=%u%%)",
                idx, (double)air_target, (double)air_hum, (unsigned)menu.storage_hum[idx], (unsigned)menu.storage_rh_hyst[idx]);
      } else {
        air_target = NAN; // греем = off
        if (heater_temp < 80.0f) {
          fan_on_ = false;
          driveFan(fan_on_);
        }
        DEBUG_I("[MODE][%d] Storage: control=HUM, heating=0, air_target=OFF "
                "(storageTemp=%.1fC RH=%.1f%% targetRH=%u%% hystRH=%u%%)",
                idx, (double)air_target, (double)air_hum, (unsigned)menu.storage_hum[idx], (unsigned)menu.storage_rh_hyst[idx]);
      }
    } else {
      air_target = storageTargetByTemperature(idx);
      updateFanSimple(inputs_.heaterTempC);
      DEBUG_I("[MODE][%d] Storage: control=TEMP targetTemp_DRY=%.1fC storageTemp=%.1fC", idx, (double)targetTemp_, (double)air_target);
    }
  }

  // if there is no target, turn off the heating and exit
  if (!isfinite(air_target)) {
    driveHeater(0); //! TEST
    return;
  }

  uint32_t dt_ms = nowMs - pidPrevMs_;

  // === synchronization of PID calculation frequency ===
  if ((uint32_t)(nowMs - pidPrevMs_) < pidUpdateIntervalMs_) {
    updateServoSmooth();
    DEBUG_T("[PIDGATE][%d] now=%lu prev=%lu int=%lu Δ=%lu", index(), (unsigned long)nowMs, (unsigned long)pidPrevMs_, (unsigned long)pidUpdateIntervalMs_, (unsigned long)(nowMs - pidPrevMs_));
    return;
  }

  float dt_s = dt_ms * 0.001f;
  pidPrevMs_ = nowMs;
  const float nowSec = nowMs / 1000.0f;
  DEBUG_T("[PID][%d] update, dt = %.3f s", index(), (double)dt_s);

  /** === PID cascade === */
  float deltaC = menu.delta_c[idx]; // общая дельта по температуре
  bool delta_is_percent = menu.heater_delta_is_percent[idx];

  if (delta_is_percent) deltaC = air_target * deltaC / 100.0f;
  deltaC = (targetTemp_ + deltaC <= menu.heater_max_temp[idx]) ? deltaC : menu.heater_max_temp[idx] - targetTemp_;

  heater_setpoint_ = computeHeaterTargetFromAir(air_temp, heater_temp, air_target, deltaC, nowSec);

  heater_setpoint_ = heater_setpoint_ < menu.heater_max_temp[idx] ? heater_setpoint_ : menu.heater_max_temp[idx];

  uint8_t heatingBoostBand = 5; // TODO: necessary verification
  // if (air_temp + heatingBoostBand <= air_target) heater_setpoint_ = air_target + deltaC;

  // heater_setpoint_ = 75.0f;
  // DEBUG_W("heater_setpoint_=%.2f", (double)heater_setpoint_);
  float duty01 = runHeaterInnerPID_ToDuty01(heater_temp, heater_setpoint_, nowSec, menu.heater_max_temp[idx]);
  // DEBUG_W("tick  --------------duty01:   %.2f", (double)duty01);

  bool inProfileRamp = (mode_ == DryerMode::Profile &&
                        profileState_.phase == ProfileState::Phase::Ramp);
  if (!inProfileRamp && verifyHeaterUpdate_(nowMs, heater_temp, heater_setpoint_, duty01, idx)) return;

  // sinInc += 0.001f;
  // if (sinInc >= 2*M_PI) sinInc -= 2*M_PI;
  // duty01 = 0.5f*(sinf(sinInc)+1.0f);

  driveHeater(duty01);
  updateServoSmooth();
  //!![%d] add in log
  if (millis() - lastDryLog < 250) return;
  lastDryLog = millis();
  if (hx711MultiPtr) hx711Multi.getMassMulti(index());
  //=== TIMER ===
  if (dryStartMs_ == 0 && isDry) {
    const float err = targetTemp_ - air_temp;
    const bool onTargetNow = fabsf(err) <= TARGET_BAND_C;
    if (onTargetNow) dryStartMs_ = nowMs;
  }
  DEBUG_T("dryStartMs_ %d", dryStartMs_);
  DEBUG_OP("[%d] AirT:%.1f→%.1fC(e:%.1f) [aP:%.2f aI:%.2f aD:%.2f aGm:%.3f]"
           " HeatT:%.1f→%.1fC(e:%.1f) [hP:%.2f hI:%.2f hD:%.2f hGm:%.3f] duty:%.0f%%",
           idx, (double)air_temp, (double)air_target, (double)(air_target - air_temp), (double)pid_air_.proportional, (double)pid_air_.integral, (double)pid_air_.deriv, (double)pid_air_.gain_multiplier, (double)heater_temp, (double)heater_setpoint_, (double)(heater_setpoint_ - heater_temp), (double)pid_heater_.proportional, (double)pid_heater_.integral, (double)pid_heater_.deriv,
           (double)pid_heater_.gain_multiplier, (double)(duty01 * 100.0f));
}


float DryerController::computeHeaterTargetFromAir(float air_temp, float heater_temp, float air_target, float deltaC, float nowSec) {
  pid_air_.output_max = air_target + deltaC;
  float heater_sp = PID_Compute(&pid_air_, air_temp, air_target, nowSec);
  return heater_sp;
}


float DryerController::runHeaterInnerPID_ToDuty01(float heater_temp, float heater_target, float nowSec, float heater_abs_maxC) {
  float pwm_percent = PID_Compute(&pid_heater_, heater_temp, heater_target, nowSec);

  float d = pwm_percent / 100.0f;
  // DEBUG_W("  -------------------> PID out=%.1f%% out_map=%.4f%%", (double)pwm_percent, (double)d);
  if (d < 0) d = 0;
  if (d > 1) d = 1;
  return d;
}


void DryerController::storageControlByHumidity(uint32_t nowMs, float air_hum, uint8_t idx) {
  if (!isfinite(air_hum)) return;

  // целевые значения из меню
  const float rhTarget = (float)menu.storage_hum[idx];                       // целевая RH, %
  const float rhHyst = (float)menu.storage_rh_hyst[idx];                     // гистерезис, %
  const uint32_t holdMs = (uint32_t)menu.storage_min_hold_sec[idx] * 1000UL; // сек -> мс

  DEBUG_E("nowMs=%u, storageLastToggleMs_=%u, nowMs-storageLastToggleMs=%u holdMs=%u", nowMs, storageLastToggleMs_, nowMs - storageLastToggleMs_, holdMs);

  // антидребезг по времени
  if (nowMs - storageLastToggleMs_ < holdMs) return;
  // DEBUG_E("--------------->");

  if (!storageHeating_) {
    if (air_hum >= rhTarget + rhHyst) {
      storageHeating_ = true;
      storageLastToggleMs_ = nowMs;
      if (lastStorageOnMs_ == 0) {
        lastStorageOnMs_ = storageLastToggleMs_;
      }
    }
  } else {
    // if (air_hum <= rhTarget - rhHyst) {
    if (nowMs - lastStorageOnMs_ > 3 * 60 * 1000) needHeatingBoost_ = true;
    if (air_hum <= rhTarget) {
      storageHeating_ = false;
      if (needHeatingBoost_) {
        needHeatingBoost_ = false;
        lastStorageOnMs_ = 0;
      }
      storageLastToggleMs_ = nowMs;
    }
  }
}

// Calculate the target air temperature for storage based on temperature
float DryerController::storageTargetByTemperature(uint8_t idx) {
  // false = абсолютная; true = % от T сушки
  const bool byPercent = menu.storage_dry_temp_mode[idx];
  const bool autoDry = menu.storage_auto_dry[idx];

  float t_air = 0.0f;
  if (byPercent) {
    const uint8_t pct = menu.storage_dry_temp_by_dry_temp[idx]; // % от T сушки
    t_air = lastDryTemp_ * (pct / 100.0f);
  } else {
    t_air = (float)menu.storage_temp[idx]; // абсолютная T из меню
  }

  if (needHeatingBoost_) {
    t_air = lastDryTemp_;
    DEBUG_E("Heating Boost targetTemp_ = %d", lastDryTemp_);
  }

  const float lo = 45.0f;
  const float hi = menu.air_max_temp[idx];
  if (t_air < lo) t_air = lo;
  if (t_air > hi) t_air = hi;
  return t_air;
}

void DryerController::applyServoFromMenu(uint8_t idx, bool apply_now) {
  auto clamp = [](int v) { return v < 0 ? 0 : (v > 180 ? 180 : v); };
  int c = clamp(menu.servo_closed_angle[idx]);
  int o = clamp(menu.servo_open_angle[idx]);
  setServoLimits((uint8_t)c, (uint8_t)o);
  if (apply_now) setServoTarget_(angleClosed_, /*immediate=*/true);
}

void DryerController::driveHeater(float power01) {
  lastDuty01_ = clampf(power01, 0.0f, 1.0f);

  if (mode_ != DryerMode::Drying && mode_ != DryerMode::Storage && mode_ != DryerMode::Profile && mode_ != DryerMode::PidAutoTune) power01 = 0.0f;
  if (power01 < 0.0f) power01 = 0.0f;
  if (power01 > 1.0f) power01 = 1.0f;


#ifdef USE_SOFT_PWM
  auto &sp = SoftPwmManager::instance();
  const float dutyPct = power01 * 100.0f;


  if (power01 <= 0.0f) {
    if (pwmChannel_ >= 0) {
      sp.setDuty(pwmChannel_, 0.0f);
      sp.removeChannel((uint8_t)pwmChannel_);
      pwmChannel_ = -1;
    }
    pinMode(heaterPin_, OUTPUT);
    digitalWrite(heaterPin_, HEATER_OFF_LEVEL);
    DEBUG_T("[CTRL][%d] HEATER OFF", index());
    return;
  }

  if (pwmChannel_ < 0) {
    DEBUG_T("[CTRL][%d] addChannel", index());
    pwmChannel_ = sp.addChannel((uint32_t)heaterPin_, (float)pwmFreq_, dutyPct);
    if (pwmChannel_ < 0) {
      DEBUG_E("[CTRL][%d] addChannel failed (pin=%d, freq=%.1f, duty=%.0f%%)", index(), heaterPin_, (float)pwmFreq_, dutyPct);
      pinMode(heaterPin_, OUTPUT);
      digitalWrite(heaterPin_, HEATER_OFF_LEVEL);
      return;
    }
    DEBUG_T("[CTRL][%d] attach softPWM ch=%d pin=%d", index(), pwmChannel_, heaterPin_);
  } else if (1 /* // TODO: Обновлять при изменении */) {
    sp.setDuty(pwmChannel_, dutyPct); // 0..100%
  }

#else // ---- HW PWM ветка ----
  const uint32_t duty = (uint32_t)(power01 * pwmRange_ + 0.5f);

  if (duty == 0) {
    analogWrite(heaterPin_, 0);
    pinMode(heaterPin_, OUTPUT);
    digitalWrite(heaterPin_, HEATER_OFF_LEVEL);
    DEBUG_I("[CTRL] HEATER OFF (hwPWM)");
    return;
  } else {
    analogWrite(heaterPin_, duty);
    DEBUG_I("[CTRL] HEATER duty=%.0f%%", power01 * 100.0f);
  }

#endif
}

void DryerController::updateFanSimple(float tempC) {
  if (isnan(tempC)) return;

  const uint8_t idx = index();
  // DEBUG_I("[[%d]]fan_temp_on_c = %.1f", idx, (double)menu.fan_temp_on_c[idx]);
  const float t_on = menu.fan_temp_on_c[idx];
  const float hyst = menu.fan_hyst_c[idx];
  const float t_off = t_on - hyst;

  bool want_on = fan_on_ ? (tempC > t_off) : (tempC >= t_on);

  // Диагностика раз в N секунд или при смене состояния:
  static uint32_t lastLogMs = 0;
  uint32_t now = millis();
  if (want_on != fan_on_ || (now - lastLogMs) > 3000) {
    lastLogMs = now;
    DEBUG_T("[FANCHK][%u] T_heater=%.2f t_on=%.1f hyst=%.1f t_off=%.1f "
            "fan_on=%d -> want_on=%d",
            idx, tempC, t_on, hyst, t_off, fan_on_, want_on);
  }

  if (want_on != fan_on_) {
    fan_on_ = want_on;
    driveFan(fan_on_);
  }
}

void DryerController::driveFan(bool on) {
  fan_on_ = on;
  // DEBUG_I("[%d] Fan %s", index(), on ? "ON" : "OFF");
  digitalWrite(fanPin_, on ? HIGH : LOW);
}

void DryerController::setServoTarget_(int angle, bool immediate) {
  angle = clampAngle_(angle);
  int oldTgt = servoTargetAngle_;
  int oldCur = servoCurrentAngle_;

  if (immediate) {
    servoTargetAngle_ = angle;
    servoCurrentAngle_ = angle;
    flapServo_.write(servoCurrentAngle_);
    // обновляем физическое состояние и таймер сразу
    bool prevOpen = flap_is_open_;
    int dOpen = abs(servoCurrentAngle_ - (int)angleOpen_);
    int dClose = abs(servoCurrentAngle_ - (int)angleClosed_);
    flap_is_open_ = (dOpen + 2 < dClose);
    if (flap_is_open_ != prevOpen) lastServoMs = millis();
    DEBUG_T("[SERVO_SET][%d] immediate=1 cur:%d->%d tgt:%d->%d", index(), oldCur, servoCurrentAngle_, oldTgt, servoTargetAngle_);
  } else {
    if (servoTargetAngle_ != angle) {
      servoTargetAngle_ = angle;
      // заставим updateServoSmooth() начать движение сразу
      lastServoStepMs_ = 0;
      DEBUG_T("[SERVO_SET][%d] immediate=0 tgt:%d->%d (cur=%d)", index(), oldTgt, servoTargetAngle_, oldCur);
    } else {
      DEBUG_T("[SERVO_SET][%d] immediate=0 tgt:UNCHANGED=%d (cur=%d)", index(), servoTargetAngle_, oldCur);
    }
  }
}

void DryerController::setFlapOpen(bool open) { setServoTarget_(open ? angleOpen_ : angleClosed_, /*immediate=*/false); }

void DryerController::updateServoSmooth() {
  uint32_t now = millis();
  const uint32_t stepInterval = 200; // мс между шагами (скорость)
  const int stepSize = 1;            // градусов за шаг

  if (now - lastServoStepMs_ < stepInterval) return;
  lastServoStepMs_ = now;

  int delta = servoTargetAngle_ - servoCurrentAngle_;
  if (delta == 0) return; // уже на цели

  // шаг строго в сторону цели, без перескока
  int step = (delta > 0) ? stepSize : -stepSize;
  if (abs(delta) < abs(step)) step = delta; // последний неполный шаг

  int before = servoCurrentAngle_;
  servoCurrentAngle_ += step;
  flapServo_.write(servoCurrentAngle_);

  DEBUG_T("[SERVO_STEP][%d] %d -> %d (tgt=%d, step=%d)", index(), before, servoCurrentAngle_, servoTargetAngle_, step);

  // Установка фактического состояния заслонки по физическому положению
  bool prevOpen = flap_is_open_;
  int dOpen = abs(servoCurrentAngle_ - (int)angleOpen_);
  int dClose = abs(servoCurrentAngle_ - (int)angleClosed_);
  flap_is_open_ = (dOpen + 2 < dClose);
  // таймер стартует в момент физической смены состояния
  if (flap_is_open_ != prevOpen) lastServoMs = now;
}

void DryerController::setFlapLevel(float level01) {
  if (level01 < 0.0f) level01 = 0.0f;
  if (level01 > 1.0f) level01 = 1.0f;
  int angle = angleClosed_ + (int)((angleOpen_ - angleClosed_) * level01 + 0.5f);
  setServoTarget_(angle, /*immediate=*/false);
}

void DryerController::previewServoAngle(uint8_t angle) {
  DEBUG_T("[PREVIEWSERVOANGLE][%d] angle = %u", index(), angle);
  setServoTarget_(angle, /*immediate=*/true);
}

void DryerController::updateServoTimers_(uint32_t nowMs, uint32_t openTimeMs, uint32_t closeTimeMs) {
  // Страховка: интерпретация нулевых таймеров
  //  - openTimeMs == 0  → держим ЗАКРЫТО (если close>0)
  //  - closeTimeMs == 0 → держим ОТКРЫТО (если open>0)
  if (openTimeMs == 0 && closeTimeMs > 0) {
    if (flap_is_open_) {
      DEBUG_T("[SERVO_CMD][%d] TIMER force CLOSE (openTime=0)", index());
      setFlapOpen(false);
    }
  } else if (closeTimeMs == 0 && openTimeMs > 0) {
    if (!flap_is_open_) {
      DEBUG_T("[SERVO_CMD][%u] TIMER force OPEN (closeTime=0)", index());
      setFlapOpen(true);
    }
  } else {
    // В один тик допускаем только один переход
    if (flap_is_open_) {
      if (openTimeMs > 0 && (nowMs - lastServoMs) >= openTimeMs) {
        DEBUG_T("[SERVO_CMD][%u] TIMER close (immediate=0)", index());
        setFlapOpen(false);
      }
    } else { // закрыто
      if (closeTimeMs > 0 && (nowMs - lastServoMs) >= closeTimeMs) {
        DEBUG_T("[SERVO_CMD][%d] TIMER open (immediate=0)", index());
        setFlapOpen(true);
      }
    }
  }
}


// ---- ring ops RH_SAVE_PERIOD_MS ----
void DryerController::rhPush(float rh, uint32_t nowMs) {
  if (nowMs < rhRing_.nextPushMs) return;
  rhRing_.nextPushMs = nowMs + RH_SAVE_PERIOD_MS;

  // float T = inputs_.airTempC;
  // float es_kPa = 0.61094f * expf(17.625f * T / (T + 243.04f)); // kPa
  // float e_kPa  = (rh * 0.01f) * es_kPa;                        // kPa
  // float ah     = 2.1674f * e_kPa / (273.15f + T);              // g/m^3

  // rhRing_.buf[rhRing_.head] = ah;
  rhRing_.buf[rhRing_.head] = rh;

  rhRing_.head = (rhRing_.head + 1) % RH_HISTORY_LEN;
  if (rhRing_.count < RH_HISTORY_LEN) rhRing_.count++;
}

RhTrend rh_trend_linear(const RhRing &r, int N) {
  RhTrend out{0, 0, 0, 0};
  if (r.count < 2) return out;
  if (N > r.count) N = r.count;
  if (N < 2) return out;

  // x равномерная сетка по минутам: 0, Δt, 2Δt, ... (от старого к новому)
  const double dt_min = (double)RH_SAVE_PERIOD_MS / 60000.0;

  // Суммы для линрегрессии
  double Sx = 0, Sy = 0, Sxx = 0, Sxy = 0, Syy = 0;

  // сбор статистик для линейной регрессии по методу наименьших квадратов
  // Идём от старого к новому: старый = N-1, новый = 0
  for (int k = 0; k < N; ++k) {
    int idx = (r.head - N + k + RH_HISTORY_LEN) % RH_HISTORY_LEN;
    double x = k * dt_min; // минуты с равным шагом
    double y = r.buf[idx];
    Sx += x;
    Sy += y;
    Sxx += x * x;
    Sxy += x * y;
    Syy += y * y;
  }

  double denom = N * Sxx - Sx * Sx;
  if (denom == 0.0) return out;
  double a = (N * Sxy - Sx * Sy) / denom; // slope в %RH/мин
  double b = (Sy - a * Sx) / N;

  // коэффициент детерминации
  //  •	R^2 = 1.0 → идеальная прямая линия, все точки лежат точно на ней.
  //  •	R^2 \approx 0.8-0.9 → линия хорошо объясняет тренд.
  //  •	R^2 \approx 0.2-0.3 → данные шумные, линия почти ничего не объясняет.
  //  •	R^2 = 0 → линия не лучше, чем просто среднее значение.
  double ss_tot = Syy - (Sy * Sy) / N;
  double ss_res = 0.0;
  for (int k = 0; k < N; ++k) {
    int idx = (r.head - N + k + RH_HISTORY_LEN) % RH_HISTORY_LEN;
    double x = k * dt_min;
    double y = r.buf[idx];
    double yhat = a * x + b;
    double e = y - yhat;
    ss_res += e * e;
  }
  double r2 = (ss_tot <= 1e-12) ? 0.0 : fmax(0.0, 1.0 - ss_res / ss_tot);

  // ATR (средняя |Δ|, приведённая к %/мин)
  double atr_sum = 0.0;
  for (int k = 1; k < N; ++k) {
    int i2 = (r.head - N + k + RH_HISTORY_LEN) % RH_HISTORY_LEN;
    int i1 = (r.head - N + k - 1 + RH_HISTORY_LEN) % RH_HISTORY_LEN;
    atr_sum += fabs((double)r.buf[i2] - (double)r.buf[i1]);
  }
  double avg_step = atr_sum / (N - 1); // %RH за шаг
  double atr_per_min = (dt_min > 0) ? (avg_step / dt_min) : 0.0;

  // R² ≈ 1 → данные почти идеально лежат на линии - тренд надёжный. Используется как фильтр доверия
  // R² ≈ 0 → данные хаотичны, линии доверять нельзя (шум, перелом). Используется как фильтр доверия
  // Если |slope| < 0.5·ATR - плато.
  // Если |slope| > 1.5·ATR - явный тренд.
  out.slope_per_min = (float)a;
  out.r2 = (float)r2;
  out.atr_per_min = (float)atr_per_min;
  out.npts = N;
  return out;
}

// ---- решения по тренду ----
// static inline float thr_up(const RhTrend& tr, const TrendCfg& c){
//   return fmaxf(c.slope_hard_min, c.atr_up_k * tr.atr_per_min);
// }
// static inline float thr_plateau(const RhTrend& tr, const TrendCfg& c){
//   return fmaxf(0.01, c.atr_plateau_k * tr.atr_per_min);
// }

TrendPhase DryerController::decideByTrend(const RhTrend &tr) const {
  if (tr.npts < trendCfg_.min_points) return TrendPhase::Unknown;

  // Нормализованный наклон по шуму (SNR): Z = slope / max(ATR, floor)
  const float S = tr.slope_per_min;                             // %RH/мин
  const float ATR = fmaxf(tr.atr_per_min, trendCfg_.atr_floor); // %RH/мин
  // const float Z    = S / ATR;
  const float Z = S;

  // Ассиметричные пороги по направлению тренда, не зависят от OPEN/CLOSED
  const float z_up = trendCfg_.z_thr_rise;
  const float z_down = trendCfg_.z_thr_fall;

  if (S > 0 && S >= z_up) return TrendPhase::Rising;
  if (S < 0 && S <= -z_down) return TrendPhase::Falling;
  return TrendPhase::Plateau;
}

const char *DryerController::phaseToStr(TrendPhase p) {
  switch (p) {
  case TrendPhase::Rising:
    return "Rising";
  case TrendPhase::Falling:
    return "Falling";
  case TrendPhase::Plateau:
    return "Plateau";
  default:
    return "Unknown";
  }
}

void DryerController::updateDamperByTrend(uint32_t nowMs) {
  if (nowMs < trendCfg_.nextWin) return;
  trendCfg_.nextWin = nowMs + RH_TREND_UPDATE_MS;

  RhTrend tr = rh_trend_linear(rhRing_, RH_TREND_POINTS);
  TrendPhase cur = decideByTrend(tr);

  DEBUG_E("\t\t\t\t\t\t\t\t\t\tslope:%.4f, r2:%.2f, ATR:%.2f, servo:%s, prev:%s, cur:%s", tr.slope_per_min, tr.r2, tr.atr_per_min, flap_is_open_ ? "OPEN" : "CLOSED", phaseToStr(prevPhase_), phaseToStr(cur));

  if (stateSinceMs_ == 0) stateSinceMs_ = nowMs;

  const uint32_t MIN_OPEN_MS = 40UL * 1000UL;
  const uint32_t MIN_CLOSED_MS = 60UL * 1000UL;

  const bool canOpen = (!flap_is_open_) && ((nowMs - stateSinceMs_) >= MIN_CLOSED_MS);
  const bool canClose = (flap_is_open_) && ((nowMs - stateSinceMs_) >= MIN_OPEN_MS);

  switch (cur) {
  case TrendPhase::Rising:
    flap_want_open_ = false;
    break;
  case TrendPhase::Falling:
    flap_want_open_ = false;
    break;
  case TrendPhase::Plateau:
    if (prevPhase_ == TrendPhase::Rising) flap_want_open_ = true;
    if (prevPhase_ == TrendPhase::Falling) flap_want_open_ = true;
    if (prevPhase_ == TrendPhase::Unknown) flap_want_open_ = false;
    break;

  default:
    break;
  }

  if (flap_want_open_ && !flap_is_open_ && canOpen) {
    setFlapOpen(true);
    // setFlapLevel(fabsf(tr.slope_per_min));
    stateSinceMs_ = nowMs;
  } else if (!flap_want_open_ && flap_is_open_ && canClose) {
    setFlapOpen(false);
    stateSinceMs_ = nowMs;
  }
  prevPhase_ = cur;
}


void DryerController::startPidAutoTune(uint16_t targetTemp, uint8_t cycles, AutoTuneTarget which, float bandC, float power01) {
  DEBUG_E("[PID][AUTO][%d] start: target=%.1fC cycles=%u sensor=%s band=%.1fC maxPower=%.0f%%", index(), (double)targetTemp, (unsigned)cycles, (which == AutoTuneTarget::HeaterTemp) ? "Heater" : "Air", (double)bandC, (double)(power01 * 100.0f));

  auto_ = PidAutoState{}; // сброс
  auto_.active = true;
  auto_.heating = true;
  auto_.targetSel = which;
  auto_.targetC = (float)targetTemp;
  auto_.bandC = bandC;
  auto_.maxPower01 = constrain(power01, 0.0f, 1.0f);
  auto_.needCycles = (cycles == 0) ? 8 : cycles;
  auto_.minHoldMs = 4000;
  auto_.lastToggleMs = millis();
  auto_.lastStepMs = auto_.lastToggleMs;
  autoApplied_ = false;

  if (which == AutoTuneTarget::HeaterTemp) {
    // Внутренний контур: классический relay test напрямую по мощности нагревателя.
    auto_.relayLow = 0.0f;
    auto_.relayHigh = auto_.maxPower01;
  } else {
    // Внешний контур: relay test не по мощности, а по уставке внутреннего heater PID.
    uint8_t idx = index();
    float deltaC = menu.delta_c[idx];
    if (menu.heater_delta_is_percent[idx]) deltaC = auto_.targetC * deltaC / 100.0f;
    if (auto_.targetC + deltaC > menu.heater_max_temp[idx]) deltaC = menu.heater_max_temp[idx] - auto_.targetC;
    if (deltaC < 5.0f) deltaC = 5.0f;
    auto_.relayLow = auto_.targetC;
    auto_.relayHigh = clampf(auto_.targetC + deltaC, auto_.relayLow + 1.0f, (float)menu.heater_max_temp[idx]);
    PID_ResetState(&pid_heater_);
  }

  mode_ = DryerMode::PidAutoTune;
  enterAutoTuneMode_();
  if (which == AutoTuneTarget::HeaterTemp) {
    driveHeater(auto_.maxPower01);
  } else {
    const float heaterTarget = auto_.heating ? auto_.relayHigh : auto_.relayLow;
    const float nowSec = auto_.lastToggleMs / 1000.0f;
    const float duty01 = runHeaterInnerPID_ToDuty01(inputs_.heaterTempC, heaterTarget, nowSec, menu.heater_max_temp[index()]);
    driveHeater(duty01);
  }
}

inline float DryerController::clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

void DryerController::tickPidAutoTune(uint32_t nowMs) {
  if (!auto_.active) return;

  // выбираем какой датчик калибруем
  float temp = (auto_.targetSel == AutoTuneTarget::HeaterTemp) ? inputs_.heaterTempC : inputs_.airTempC;
  if (!isfinite(temp)) return;

  auto applyAutoDrive = [&](bool heating) {
    if (auto_.targetSel == AutoTuneTarget::HeaterTemp) {
      // Heater autotune: реле напрямую включает/выключает мощность.
      driveHeater(heating ? auto_.maxPower01 : 0.0f);
      return;
    }

    if (!isfinite(inputs_.heaterTempC)) {
      driveHeater(0.0f);
      return;
    }

    // Air autotune: реле переключает heater setpoint, а внутренний PID сам считает duty.
    const float heaterTarget = heating ? auto_.relayHigh : auto_.relayLow;
    const float nowSec = nowMs / 1000.0f;
    const float duty01 = runHeaterInnerPID_ToDuty01(inputs_.heaterTempC, heaterTarget, nowSec, menu.heater_max_temp[index()]);
    driveHeater(duty01);
  };

  // --- фиксированный шаг тюнинга (гейтинг по времени) ---
  if ((uint32_t)(nowMs - auto_.lastStepMs) < pidUpdateIntervalMs_) {
    // поддерживаем текущую фазу
    applyAutoDrive(auto_.heating);
    return;
  }
  auto_.lastStepMs = nowMs;

  // Трекинг экстремумов в текущей фазе
  if (auto_.heating) {
    if (temp > auto_.curMax) {
      auto_.curMax = temp;
      auto_.curMaxTime = nowMs;
    }
  } else {
    if (temp < auto_.curMin) {
      auto_.curMin = temp;
      auto_.curMinTime = nowMs;
    }
  }

  // гистерезис по времени (антидребезг фаз)
  if (nowMs - auto_.lastToggleMs < auto_.minHoldMs) {
    // просто поддерживаем текущую фазу
    applyAutoDrive(auto_.heating);
    return;
  }

  const float hi = auto_.targetC + auto_.bandC;
  const float lo = auto_.targetC - auto_.bandC;

  // фиксация пиков/впадин при смене фазы
  if (auto_.heating && temp >= hi) {
    // завершили нагрев — записываем ДЕЙСТВИТЕЛЬНЫЙ максимум
    if (auto_.peaks < PidAutoState::MAX_PEAKS) {
      auto_.maxTemps[auto_.peaks] = auto_.curMax;
      auto_.maxTimes[auto_.peaks] = auto_.curMaxTime ? auto_.curMaxTime : nowMs;
    }
    auto_.heating = false;
    auto_.lastToggleMs = nowMs;
    // сброс текущего минимума для фазы охлаждения
    auto_.curMin = INFINITY;
    auto_.curMinTime = 0;

  } else if (!auto_.heating && temp <= lo) {
    // завершили охлаждение — записываем ДЕЙСТВИТЕЛЬНЫЙ минимум
    if (auto_.peaks < PidAutoState::MAX_PEAKS) {
      auto_.minTemps[auto_.peaks] = auto_.curMin;
      auto_.minTimes[auto_.peaks] = auto_.curMinTime ? auto_.curMinTime : nowMs;
    }
    auto_.peaks++;
    auto_.heating = true;
    auto_.lastToggleMs = nowMs;
    // сброс текущего максимума для следующей фазы нагрева
    auto_.curMax = -INFINITY;
    auto_.curMaxTime = 0;

    // Достаточно циклов → считаем параметры
    if (auto_.peaks >= auto_.needCycles) {
      const uint8_t n = auto_.peaks;

      // Период Tu — среднее по временам последовательных максимумов
      // Игнорируем первые циклы как разгон (IGN), усредняем по остальным
      const uint8_t IGN = 2;
      float Tu_ms_sum = 0.0f;
      int Tu_cnt = 0;
      for (uint8_t i = (uint8_t)(IGN + 1); i < n; ++i) {
        if (auto_.maxTimes[i] && auto_.maxTimes[i - 1]) {
          Tu_ms_sum += (float)(auto_.maxTimes[i] - auto_.maxTimes[i - 1]);
          ++Tu_cnt;
        }
      }
      const float Tu = (Tu_cnt > 0) ? (Tu_ms_sum / Tu_cnt) / 1000.0f : 0.0f;

      // Амплитуда температуры a = среднее (max[i] - min[i]) / 2
      // Берём последние AVG_TAIL циклов после разгона
      const uint8_t AVG_TAIL = 5;
      uint8_t start = n > (AVG_TAIL + IGN) ? (uint8_t)(n - AVG_TAIL) : IGN;
      float a_sum = 0.0f;
      int a_cnt = 0;
      for (uint8_t i = start; i < n; ++i) {
        const float tmax = auto_.maxTemps[i];
        const float tmin = auto_.minTemps[i];
        if (isfinite(tmax) && isfinite(tmin) && (tmax != 0.0f || tmin != 0.0f)) {
          a_sum += 0.5f * fabsf(tmax - tmin);
          ++a_cnt;
        }
      }
      const float a = (a_cnt > 0) ? (a_sum / a_cnt) : 0.0f;

      // Эквивалентная амплитуда реле d = (u_high - u_low)/2.
      const float d = 0.5f * fabsf(auto_.relayHigh - auto_.relayLow);

      // Heater autotune даёт Ku в шкале мощности 0..1, а рабочий PID управляет процентами 0..100.
      // Air autotune уже работает в °C heater setpoint, здесь дополнительное масштабирование не нужно.
      const float inputScale = (auto_.targetSel == AutoTuneTarget::HeaterTemp) ? 100.0f : 1.0f;
      const float Ku = (a > 0.0f) ? (4.0f * d / (3.1415926f * a)) * inputScale : 0.0f;

      // --- Ziegler–Nichols classic (прежний вариант) ---
      // float Kp_ZN = 0.6f * Ku;
      // float Ti_ZN = 0.5f * Tu;
      // float Td_ZN = 0.125f * Tu;
      // float Ki_ZN = (Ti_ZN > 0.0f) ? (Kp_ZN / Ti_ZN) : 0.0f;
      // float Kd_ZN = Kp_ZN * Td_ZN;

      // --- Tyreus–Luyben (TL) ---
      float Kp = 0.0f, Ki = 0.0f, Kd = 0.0f;
      if (auto_.targetSel == AutoTuneTarget::HeaterTemp) {
        // TL-PI для heater: Kd=0 для устойчивости теплового объекта
        PID_Tune_TL_heater(Ku, Tu, &Kp, &Ki, &Kd);
        // Kd = 0.0f;
      } else {
        PID_Tune_TL_air(Ku, Tu, 0.6f, &Kp, &Ki, &Kd); // внешний контур консервативнее
      }

      auto_.result.ok = isfinite(Kp) && isfinite(Ki) && isfinite(Kd) && (Tu > 0.0f);
      auto_.result.Kp = Kp;
      auto_.result.Ki = Ki;
      auto_.result.Kd = Kd;
      auto_.result.Ku = Ku;
      auto_.result.Tu = Tu;

      auto_.resultShowUntilMs = millis() + 10000; // показываем 10 секунд

      // стоп
      auto_.active = false;
      driveHeater(0.0f);
      return;
    }
  }

  // подавать мощность согласно текущей фазе
  applyAutoDrive(auto_.heating);
}

/********* Verify Heater (Klipper-like) ********/
void DryerController::resetVerifyHeater_(uint32_t nowMs, float heater_temp) {
  vh_ = VerifyHeaterState{};
  vh_.active = true; // был валидный вызов
  vh_.lastTickMs = nowMs;
  // лестницу подготовим при арме
}

bool DryerController::verifyHeaterUpdate_(uint32_t nowMs, float heater_temp, float heater_setpoint, float duty01, uint8_t idx) {
  // Проверка времени в начале - функция работает не чаще VH_TICK_PERIOD_MS
  if ((uint32_t)(nowMs - vh_.lastTickMs) < VH_TICK_PERIOD_MS) {
    return false;
  }
  vh_.lastTickMs += VH_TICK_PERIOD_MS;

  // Базовые проверки и сброс состояния
  if (!isfinite(heater_temp) || !isfinite(heater_setpoint) || !vh_.active) {
    if (vh_.armed) {
      DEBUG_W("VH[%u] DISARMED: invalid data (T=%.1f, SP=%.1f, active=%d)", idx, heater_temp, heater_setpoint, vh_.active);
    }
    resetVerifyHeater_(nowMs, heater_temp);
    vh_.armed = false;
    return false;
  }

  // Вычисляем пороги ARM/DISARM с автоматическим гистерезисом
  const float vhArmPwmMin = menu.vh_arm_pwm_min_pct[idx] / 100.0f;
  const float vhDisarmPwmMax = fmaxf(0.0f, vhArmPwmMin - VH_DISARM_PWM_OFFSET);

  // Disarm условия
  if (vh_.armed) {
    bool shouldDisarm = false;
    const char *reason = "";

    if (duty01 < vhDisarmPwmMax) {
      shouldDisarm = true;
      reason = "duty too low";
    } else if (isfinite(vh_.snapshotTarget) && heater_setpoint <= vh_.snapshotTarget - VH_SNAPSHOT_FALL_C - VH_HYST_C) {
      // Disarm только если уставка упала на VH_SNAPSHOT_FALL_C + VH_HYST_C (6°C)
      shouldDisarm = true;
      reason = "setpoint fell significantly";
    }

    if (shouldDisarm) {
      DEBUG_W("VH[%u] DISARMED: %s (duty=%.2f<%.2f, SP=%.1f→%.1f, err=%.1f)", idx, reason, duty01, vhDisarmPwmMax, vh_.snapshotTarget, heater_setpoint, vh_.err);
      vh_.armed = false;
      vh_.approaching = false;
      vh_.err = 0.0f;
      vh_.snapshotTarget = NAN;
    }
  }

  // Arm условие
  // const float vhArmPwmMin = menu.vh_arm_pwm_min_pct[idx] / 100.0f;
  if (!vh_.armed && duty01 >= vhArmPwmMin) {
    const float vhHeatingGain = menu.vh_heating_gain[idx];
    const uint32_t vhCheckGainTimeMs = (uint32_t)menu.vh_check_gain_time_s[idx] * 1000UL;
    DEBUG_W("VH[%u] ARMED: duty=%.2f, T=%.1f, SP=%.1f, goal=+%.1f°C in %ds", idx, duty01, heater_temp, heater_setpoint, vhHeatingGain, vhCheckGainTimeMs / 1000);
    vh_.armed = true;
    vh_.err = 0.0f;
    vh_.approaching = true;
    vh_.snapshotTarget = heater_setpoint;
    vh_.snapshotAtSetpoint = heater_setpoint;
    vh_.goalTemp = heater_temp + vhHeatingGain;
    vh_.goalDeadlineMs = nowMs + vhCheckGainTimeMs;
  }

  if (!vh_.armed || !isfinite(vh_.snapshotTarget)) return false;

  // Новый снапшот при росте уставки (с гистерезисом)
  if (heater_setpoint >= vh_.snapshotTarget + VH_SNAPSHOT_RISE_C + VH_HYST_C / 2.0f) {
    const float vhHeatingGain = menu.vh_heating_gain[idx];
    const uint32_t vhCheckGainTimeMs = (uint32_t)menu.vh_check_gain_time_s[idx] * 1000UL;
    DEBUG_W("VH[%u] NEW SNAPSHOT: SP %.1f→%.1f (+%.1f°C), T=%.1f, err=%.1f (kept)", idx, vh_.snapshotTarget, heater_setpoint, heater_setpoint - vh_.snapshotTarget, heater_temp, vh_.err);
    vh_.snapshotTarget = heater_setpoint;
    vh_.snapshotAtSetpoint = heater_setpoint;
    vh_.approaching = true;
    vh_.goalTemp = heater_temp + vhHeatingGain;
    vh_.goalDeadlineMs = nowMs + vhCheckGainTimeMs;
  }

  const float lowerBand = vh_.snapshotTarget - VH_HYST_C;

  // Проверка достижения цели - ЗДЕСЬ сбрасываем err
  if (heater_temp >= lowerBand) {
    DEBUG_W("VH[%u] TARGET REACHED: T=%.1f ≥ lowerBand=%.1f, err %.1f→0", idx, heater_temp, lowerBand, vh_.err);
    vh_.approaching = false;
    vh_.err = 0.0f;
    return false;
  }

  // Интегратор ошибки
  const float vhMaxErr = menu.vh_max_err[idx];
  float deficit = lowerBand - heater_temp;
  if (deficit > 0.0f) {
    float prevErr = vh_.err;
    vh_.err += (deficit > 1.0f) ? 1.0f : deficit;

    // Логируем каждые 10 единиц ошибки или критические значения
    if ((int)vh_.err / 10 > (int)prevErr / 10 || vh_.err > vhMaxErr * 0.9f) {
      DEBUG_W("VH[%u] ERROR RISING: err %.1f→%.1f, deficit=%.1f, T=%.1f, target=%.1f", idx, prevErr, vh_.err, deficit, heater_temp, lowerBand);
    }

    if (vh_.err > vhMaxErr) {
      POST_ERROR(ERRSEV_CRITICAL, idx, ERRSRC_HEATER, ERRC_NO_RESPONSE, "Heater not heating", (int32_t)lroundf(heater_temp));
      // DEBUG_W("VH[%u] CRITICAL: err=%.1f > MAX=%.1f, T=%.1f stuck below %.1f",
      //         idx, vh_.err, vhMaxErr, heater_temp, lowerBand);
      return true;
    }
  }

  // Проверка прироста температуры (лестница)
  if (vh_.approaching) {
    const float vhHeatingGain = menu.vh_heating_gain[idx];
    const uint32_t vhCheckGainTimeMs = (uint32_t)menu.vh_check_gain_time_s[idx] * 1000UL;
    if (heater_temp >= vh_.goalTemp) {
      DEBUG_W("VH[%u] GAIN OK: T %.1f→%.1f (+%.1f°C), err %.1f→0, next goal +%.1f°C", idx, vh_.goalTemp - vhHeatingGain, heater_temp, heater_temp - (vh_.goalTemp - vhHeatingGain), vh_.err, vhHeatingGain);
      vh_.err = 0.0f;
      vh_.goalTemp = heater_temp + vhHeatingGain;
      vh_.goalDeadlineMs = nowMs + vhCheckGainTimeMs;
    } else if (nowMs >= vh_.goalDeadlineMs) {
      uint32_t overdue = (nowMs - vh_.goalDeadlineMs) / 1000;
      DEBUG_W("VH[%u] GAIN MISSED: T=%.1f < goal=%.1f after %ds (+%ds overdue)", idx, heater_temp, vh_.goalTemp, vhCheckGainTimeMs / 1000, overdue);
      vh_.approaching = false;
    }
  }
  return false;
}
