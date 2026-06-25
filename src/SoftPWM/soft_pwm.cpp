#include "soft_pwm.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_SOFT_PWM
#define LOG_TAG "SPWM"
#include "debug_log.h"


#if defined(ARDUINO_ARCH_RP2040)
  #define USING_MICROS_RESOLUTION true
  #include "RP2040_Slow_PWM.h"
  static RP2040_Timer ITimer(0);
  static RP2040_Slow_PWM ISR_PWM;
  static bool TimerHandler(struct repeating_timer *t){ (void)t; ISR_PWM.run(); return true; }
#endif

// ---------- ВНУТРЕННИЕ ХЕЛПЕРЫ / КОНСТРУКТОР ----------

SoftPwmManager& SoftPwmManager::instance() {
  static SoftPwmManager m;
  return m;
}

SoftPwmManager::SoftPwmManager() {
#if defined(ARDUINO_ARCH_RP2040)
  // Инициализация критсекции и таблиц
  critical_section_init(&cs_);
  for (int i = 0; i < kMaxChannels; ++i) {
    slots_[i].used  = false;
    slots_[i].pin   = 0xFF;
    slots_[i].freq  = 0.0f;
    slots_[i].duty  = 0.0f;
    slots_[i].libCh = -1;
    slotOfLibCh_[i] = -1;
  }
  // Если в проекте ISR_PWM.run() дергается из таймера — запустить здесь.
  // Иначе — удалите/закомментируйте это (или вызывайте run() из loop()).
  // Пример (каждые ~20-50 мкс; подберите под вашу частоту PWM):
  // ITimer.attachInterruptInterval(50, TimerHandler);
#endif
}

bool SoftPwmManager::valid(int ch) const {
  return (ch >= 0 && ch < kMaxChannels && slots_[ch].used);
}

uint8_t SoftPwmManager::pinOf(int ch) const {
  if (!valid(ch)) return 0xFF;
  return slots_[ch].pin;
}

int SoftPwmManager::findSlotByPin(uint8_t pin) const {
  for (int s = 0; s < kMaxChannels; ++s)
    if (slots_[s].used && slots_[s].pin == pin) return s;
  return -1;
}

int SoftPwmManager::allocFreeSlot() {
  for (int s = 0; s < kMaxChannels; ++s)
    if (!slots_[s].used) return s;
  return -1;
}

// ---------- ПУБЛИЧНЫЕ МЕТОДЫ --------

// ЛЕГАСИ
bool SoftPwmManager::beginBackend(uint32_t tick_us) {
#if defined(ARDUINO_ARCH_RP2040)
  if (backendStarted_) return true;

  const bool ok = ITimer.attachInterruptInterval(tick_us, TimerHandler);
  if (!ok) {
    DEBUG_E("[SPWM] backend start FAILED (tick=%u us)", (unsigned)tick_us);
    return false;
  }

  backendStarted_ = true;
  backendTickUs_  = tick_us;
  DEBUG_I("[SPWM] backend started, tick=%u us", (unsigned)tick_us);
  return true;
#else
  (void)tick_us; 
  return false;
#endif
}



int SoftPwmManager::begin(uint8_t pin, float freq, float duty) {
#if defined(ARDUINO_ARCH_RP2040)
  if (freq <= 0) freq = 1;
  if (duty < 0) duty = 0;
  if (duty > 100) duty = 100;

  critical_section_enter_blocking(&cs_);

  // 1) Если уже есть — просто обновим и вернём тот же слот
  int s = findSlotByPin(pin);
  if (s >= 0) {
    ISR_PWM.setPWM(pin, freq, duty);
    slots_[s].freq = freq;
    slots_[s].duty = duty;
    critical_section_exit(&cs_);
    DEBUG_I("[SPWM] reuse slot=%d pin=%d f=%.1f d=%.1f%%", s, pin, freq, duty);
    return s;
  }

  // 2) Иначе создаём новый слот
  int sfree = allocFreeSlot();
  if (sfree < 0) {
    critical_section_exit(&cs_);
    DEBUG_E("[SPWM] no free slots for pin=%d", pin);
    return -1;
  }

  // Просим канал у библиотеки
  int libCh = ISR_PWM.setPWM(pin, freq, duty);
  if (libCh < 0 || libCh >= kMaxChannels) {
    critical_section_exit(&cs_);
    DEBUG_E("[SPWM] setPWM failed pin=%d f=%.1f d=%.1f%% (libCh=%d)",
            pin, freq, duty, libCh);
    return -1;
  }

  // Заполняем слот и обратную мапу
  slots_[sfree].used  = true;
  slots_[sfree].pin   = pin;
  slots_[sfree].freq  = freq;
  slots_[sfree].duty  = duty;
  slots_[sfree].libCh = libCh;
  slotOfLibCh_[libCh] = sfree;

  critical_section_exit(&cs_);
  DEBUG_I("[SPWM] attach slot=%d libCh=%d pin=%d f=%.1f d=%.1f%%",
          sfree, libCh, pin, freq, duty);
  return sfree;
#else
  (void)pin; (void)freq; (void)duty;
  return -1;
#endif
}

// Современный путь: враппер над begin, чтобы код с addChannel тоже работал.
int SoftPwmManager::addChannel(uint8_t pin, float freq, float duty) {
  return begin(pin, freq, duty); // идемпотентен по pin
}

void SoftPwmManager::setDuty(int ch, float duty) {
#if defined(ARDUINO_ARCH_RP2040)
  if (duty < 0) duty = 0;
  if (duty > 100) duty = 100;

  critical_section_enter_blocking(&cs_);
  if (!valid(ch)) {
    critical_section_exit(&cs_);
    DEBUG_W("[SPWM] setDuty on invalid slot=%d (ignored)", ch);
    return;
  }
  auto &s = slots_[ch];
  // ISR_PWM.setPWM(s.pin, s.freq, duty);
  ISR_PWM.modifyPWMChannel(ch, s.pin, s.freq, duty);
  s.duty = duty;
  critical_section_exit(&cs_);
#else
  (void)ch; (void)duty;
#endif
}

void SoftPwmManager::setFreq(int ch, float freq) {
#if defined(ARDUINO_ARCH_RP2040)
  if (freq <= 0) freq = 1;

  critical_section_enter_blocking(&cs_);
  if (!valid(ch)) {
    critical_section_exit(&cs_);
    DEBUG_W("[SPWM] setFreq on invalid slot=%d (ignored)", ch);
    return;
  }
  auto &s = slots_[ch];
  ISR_PWM.setPWM(s.pin, freq, s.duty);
  s.freq = freq;
  critical_section_exit(&cs_);
#else
  (void)ch; (void)freq;
#endif
}

void SoftPwmManager::removeChannel(int ch) {
#if defined(ARDUINO_ARCH_RP2040)
  critical_section_enter_blocking(&cs_);
  if (!valid(ch)) {
    critical_section_exit(&cs_);
    return; // идемпотентно
  }
  int libCh = slots_[ch].libCh;
  ISR_PWM.deleteChannel(libCh);
  slotOfLibCh_[libCh] = -1;
  slots_[ch].used  = false;
  slots_[ch].pin   = 0xFF;
  slots_[ch].freq  = 0.0f;
  slots_[ch].duty  = 0.0f;
  slots_[ch].libCh = -1;
  critical_section_exit(&cs_);

  DEBUG_I("[SPWM] removed slot=%d libCh=%d", ch, libCh);
#else
  (void)ch;
#endif
}








// bool SoftPwmManager::begin(uint32_t us){
// #if defined(ARDUINO_ARCH_RP2040)
//   if (started_) return true;
//   // тест: базовый тик таймера для софт-PWM (чем меньше - тем точнее duty, но выше нагрузка ISR)
//   if (!ITimer.attachInterruptInterval(us, TimerHandler)) return false;
//   started_ = true;
//   return true;
// #else
//   (void)us; return false;
// #endif
// }


// int SoftPwmManager::addChannel(uint32_t pin, float freq, float duty){
// #if defined(ARDUINO_ARCH_RP2040)
//   if (!started_) return -1;

//   // find a free internal index
//   int idx = -1;
//   for (int i = 0; i < kMaxChannels; ++i) {
//     if (!chUsed_[i]) { idx = i; break; }
//   }
//   if (idx < 0) return -1;

//   // создаём канал в библиотеке
//   DEBUG_I("addChannel pin=%u, freq=%.1f, duty=%.1f", (unsigned)pin, freq, duty);

//   int lib = ISR_PWM.setPWM(pin, freq, duty);
//   if (lib < 0) return -1;

//   // сохраняем привязку
//   chUsed_[idx] = true;
//   chPin_[idx]  = pin;
//     DEBUG_I("chPin_[idx] =%u", chPin_[idx]);
//   chFreq_[idx] = freq;
//   return idx;
// #else
//   (void)pin; (void)freq; (void)duty; return -1;
// #endif
// }


// void SoftPwmManager::setDuty(int ch, float duty){
// #if defined(ARDUINO_ARCH_RP2040)
//   if (ch < 0 || ch >= kMaxChannels) return;
//   if (!chUsed_[ch]) return;
//   if (duty < 0) duty = 0; if (duty > 100) duty = 100;
//   const uint32_t pin = chPin_[ch];

//   float freq = chFreq_[ch] > 0 ? chFreq_[ch] : 1.0f;
//   // ISR_PWM.setPWM(pin, freq, duty);

//   ISR_PWM.modifyPWMChannel(ch, pin, freq, duty);
//   DEBUG_I("channel pin:%d  duty:%.0f%", pin, duty);

// #else
//   (void)ch; (void)duty;
// #endif
// }


// void SoftPwmManager::removeChannel(int ch){
// #if defined(ARDUINO_ARCH_RP2040)
//   if (ch < 0 || ch >= kMaxChannels) return; // тест: невалидный id
//   if (!chUsed_[ch]) return;                 // тест: уже удалён или не создавался
//   ISR_PWM.deleteChannel(ch);
//   chUsed_[ch] = false;
//   chFreq_[ch] = 0.0f;
// #else
//   (void)ch;
// #endif
// }
