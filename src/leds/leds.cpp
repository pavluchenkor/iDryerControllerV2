#include "leds.h"

#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <math.h>

#include "controller/control.h"
#include "hardware/hardware.h"
#include "menu/menu_state.h"
#include "service_screen/service_screen_state.h"

static Adafruit_NeoPixel strip(4, NEOPIXEL, NEO_GRB + NEO_KHZ800);

static const uint8_t NUMPIXELS = 4;
static uint32_t nextLedUpdate = 0;
static uint16_t baseHue = 0; // для радуги

// Параметры для эффекта "дыхания"
struct BreathParams {
  uint32_t color;
  uint32_t periodMs;    // период одного цикла вдох-выдох
  uint32_t startTime;   // когда началась анимация
  uint32_t repeatCount; // сколько раз повторить (0 = бесконечно)
};
static BreathParams breathParams = {0, 1000, 0, 0};

// === LedsManager ===
struct LedsIndication {
  LedsPriority priority;
  uint32_t endTime;      // millis когда индикация закончится (0 = бесконечно)
  LedsDrawFunc drawFunc; // nullptr = использовать solid color
  uint32_t solidColor;   // если drawFunc == nullptr
};

static LedsIndication currentIndication = {LedsPriority::Status, 0, nullptr, 0};

// Очередь Web-индикаций (один активный Web + до 4 ожидающих; при переполнении выкидываем самый старый)
static constexpr uint8_t WEB_QUEUE_CAP = 4;

struct WebQueuedIndication {
  bool breath;
  bool custom;
  uint32_t color;
  uint32_t periodMs;
  uint32_t durationMs;
  LedsDrawFunc drawFunc;
};

static WebQueuedIndication webQueue[WEB_QUEUE_CAP];
static uint8_t webQHead = 0;
static uint8_t webQTail = 0;
static uint8_t webQCount = 0;

static void webQueueClear() {
  webQHead = 0;
  webQTail = 0;
  webQCount = 0;
}

static void webQueuePush(const WebQueuedIndication &item) {
  if (webQCount == WEB_QUEUE_CAP) {
    webQHead = (uint8_t)((webQHead + 1U) % WEB_QUEUE_CAP);
    webQCount--;
  }
  webQueue[webQTail] = item;
  webQTail = (uint8_t)((webQTail + 1U) % WEB_QUEUE_CAP);
  webQCount++;
}

static bool webQueuePop(WebQueuedIndication *out) {
  if (webQCount == 0 || out == nullptr) return false;
  *out = webQueue[webQHead];
  webQHead = (uint8_t)((webQHead + 1U) % WEB_QUEUE_CAP);
  webQCount--;
  return true;
}

static void ledsDrawBreath(uint32_t now, void *stripPtr);

static void webStartFromQueued(const WebQueuedIndication &q) {
  if (q.breath) {
    breathParams.color = q.color;
    breathParams.periodMs = q.periodMs;
    breathParams.startTime = millis();
    breathParams.repeatCount = 0;
    currentIndication.priority = LedsPriority::Web;
    currentIndication.endTime = millis() + q.durationMs;
    currentIndication.drawFunc = ledsDrawBreath;
    currentIndication.solidColor = 0;
  } else if (q.custom) {
    currentIndication.priority = LedsPriority::Web;
    currentIndication.endTime = millis() + q.durationMs;
    currentIndication.drawFunc = q.drawFunc;
    currentIndication.solidColor = 0;
  } else {
    currentIndication.priority = LedsPriority::Web;
    currentIndication.endTime = millis() + q.durationMs;
    currentIndication.drawFunc = nullptr;
    currentIndication.solidColor = q.color;
  }
}

// Forward declarations для внутренних функций отрисовки
static void ledsDrawStatus(uint32_t now);
static void ledsDrawSolidColor(uint32_t color);

static uint8_t clampByte(float v) {
  if (v < 0.0f) v = 0.0f;
  if (v > 255.0f) v = 255.0f;
  return (uint8_t)(v + 0.5f);
}

void ledsBegin() {
  strip.begin();
  strip.clear();
  strip.show();
}

// Внутренняя функция отрисовки статуса периферии
static void ledsDrawStatus(uint32_t now) {
  if (strip.numPixels() < NUMPIXELS) return;

  DryerController *ctrl = (controllers[0]) ? controllers[0] : nullptr;

  DryerInputs in{};
  float heaterPower = 0.0f;
  bool fanOn = false;
  DryerMode mode = DryerMode::Idle;

  if (ctrl) {
    in = ctrl->getInputs();
    heaterPower = ctrl->heaterPower01();
    fanOn = ctrl->fanOn();
    mode = ctrl->mode();
  }

  strip.clear();

  bool heaterActive = (mode == DryerMode::Drying || mode == DryerMode::Storage || mode == DryerMode::PidAutoTune);
  if (heaterActive && heaterPower > 0.0f) {
    uint8_t red = clampByte(heaterPower * 255.0f);
    if (red < 10) red = 10;

    uint8_t green = 0;
    if (mode == DryerMode::Storage) {
      green = clampByte(red * 0.8f); // более насыщенный оранжевый
    }

    strip.setPixelColor(0, strip.gamma32(strip.Color(red, green, 0)));
  }

  if (fanOn) {
    const uint16_t fanPeriodMs = 4800;
    float phase = (float)(now % fanPeriodMs) / fanPeriodMs; // 0..1
    uint8_t wave = (uint8_t)((sinf(phase * TWO_PI) * 0.5f + 0.5f) * 255.0f);
    uint8_t blue = 100 + (wave >> 1); // 100..227
    uint8_t green = blue / 3;
    strip.setPixelColor(1, strip.gamma32(strip.Color(0, green, blue)));
  }

  if (isfinite(in.airHumRH)) {
    auto lerp8 = [&](uint8_t a, uint8_t b, float t) -> uint8_t { return clampByte(a + (b - a) * t); };

    const uint8_t white[3] = {200, 200, 200};
    const uint8_t green[3] = {0, 200, 0};
    const uint8_t cyan[3] = {0, 120, 255};
    const uint8_t blue[3] = {0, 0, 200};

    float rh = constrain(in.airHumRH, 0.0f, 40.0f);
    const uint32_t humColor = [&]() {
      if (rh <= 10.0f) {
        float t = rh / 10.0f;
        return strip.Color(lerp8(white[0], green[0], t), lerp8(white[1], green[1], t), lerp8(white[2], green[2], t));
      } else if (rh <= 20.0f) {
        float t = (rh - 10.0f) / 10.0f;
        return strip.Color(lerp8(green[0], cyan[0], t), lerp8(green[1], cyan[1], t), lerp8(green[2], cyan[2], t));
      } else if (rh <= 30.0f) {
        float t = (rh - 20.0f) / 10.0f;
        return strip.Color(lerp8(cyan[0], blue[0], t), lerp8(cyan[1], blue[1], t), lerp8(cyan[2], blue[2], t));
      }
      return strip.Color(blue[0], blue[1], blue[2]);
    }();

    strip.setPixelColor(2, strip.gamma32(humColor));
  }

  if (isfinite(in.airTempC)) {
    const float tMin = 45.0f;
    const float tMax = 90.0f;
    float norm = (in.airTempC - tMin) / (tMax - tMin);
    norm = constrain(norm, 0.0f, 1.0f);
    uint8_t red = clampByte(norm * 255.0f);
    uint8_t green = clampByte((1.0f - norm) * 255.0f);
    strip.setPixelColor(3, strip.gamma32(strip.Color(red, green, 0)));
  }

  strip.show();
}

// Отрисовка одного цвета на всех пикселях
static void ledsDrawSolidColor(uint32_t color) {
  strip.clear();
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.gamma32(color));
  }
  strip.show();
}

// Эффект "дыхания" - плавное изменение яркости
static void ledsDrawBreath(uint32_t now, void *stripPtr) {
  (void)stripPtr; // не используется, strip глобальный

  uint32_t elapsed = now - breathParams.startTime;
  uint32_t cycleTime = elapsed % breathParams.periodMs;

  // Вычисляем фазу: 0.0 (мин яркость) -> 1.0 (макс) -> 0.0 (мин)
  float phase = (float)cycleTime / (float)breathParams.periodMs;
  // Используем синус для плавного "дыхания"
  float brightness = (sinf(phase * TWO_PI - PI / 2.0f) + 1.0f) * 0.5f; // 0..1

  // Применяем минимальную яркость 10% чтобы цвет был виден
  brightness = 0.1f + brightness * 0.9f;

  // Извлекаем RGB компоненты из цвета
  uint8_t r = (breathParams.color >> 16) & 0xFF;
  uint8_t g = (breathParams.color >> 8) & 0xFF;
  uint8_t b = breathParams.color & 0xFF;

  // Применяем яркость
  r = clampByte(r * brightness);
  g = clampByte(g * brightness);
  b = clampByte(b * brightness);

  uint32_t dimmedColor = (r << 16) | (g << 8) | b;

  strip.clear();
  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, strip.gamma32(dimmedColor));
  }
  strip.show();
}

// === Главный менеджер индикаций ===
void ledsTick(uint32_t now) {
  if (now < nextLedUpdate) return;
  nextLedUpdate = now + 30;

  // Проверяем таймер текущей индикации
  if (currentIndication.endTime != 0 && now >= currentIndication.endTime) {
    if (currentIndication.priority == LedsPriority::Web) {
      WebQueuedIndication next{};
      if (webQueuePop(&next)) {
        webStartFromQueued(next);
      } else {
        ledsResetToStatus();
      }
    } else {
      ledsResetToStatus();
    }
  }

  // Выбираем что рисовать
  if (currentIndication.priority == LedsPriority::Status) {
    // Обычный статус периферии
    ledsDrawStatus(now);
  } else {
    // Временная индикация
    if (currentIndication.drawFunc != nullptr) {
      // Кастомная функция отрисовки
      currentIndication.drawFunc(now, &strip);
    } else {
      // Solid color
      ledsDrawSolidColor(currentIndication.solidColor);
    }
  }
}

// Плавный перелив по всем 4 диодам, с небольшим сдвигом оттенка между ними
void ledsTickRainbow(uint32_t now) {
  const uint32_t interval_ms = 1; // частота обновления (20 мс ≈ 50 Гц)
  if (now < nextLedUpdate) return;
  nextLedUpdate = now + interval_ms;

  baseHue += 256; // ~1/256 круга за тик;

  for (uint16_t i = 0; i < strip.numPixels(); i++) {
    uint16_t hue = baseHue + i * 4096 * 3; // сдвигаем hue между пикселями
    // ColorHSV(h, s, v), где h: 0..65535, s/v: 0..255
    uint32_t c = strip.ColorHSV(hue, 255, 80); // 80 — яркость (0..255)
    c = strip.gamma32(c);                      // гамма-коррекция — выглядит мягче
    strip.setPixelColor(i, c);
  }
  strip.show();
}

// === API функции ===

void ledsShowAlert(uint32_t color, uint32_t durationMs) {
  if (currentIndication.priority > LedsPriority::Alert) return; // Уже показывается что-то с более высоким приоритетом

  webQueueClear();
  currentIndication.priority = LedsPriority::Alert;
  currentIndication.endTime = millis() + durationMs;
  currentIndication.drawFunc = nullptr;
  currentIndication.solidColor = color;
}

void ledsShowAlertCustom(LedsDrawFunc drawFunc, uint32_t durationMs) {
  if (currentIndication.priority > LedsPriority::Alert) return;

  webQueueClear();
  currentIndication.priority = LedsPriority::Alert;
  currentIndication.endTime = millis() + durationMs;
  currentIndication.drawFunc = drawFunc;
  currentIndication.solidColor = 0;
}

void ledsShowAlertBreath(uint32_t color, uint32_t periodMs, uint32_t durationMs) {
  if (currentIndication.priority > LedsPriority::Alert) return;

  webQueueClear();
  breathParams.color = color;
  breathParams.periodMs = periodMs;
  breathParams.startTime = millis();
  breathParams.repeatCount = 0; // бесконечно пока не истечет durationMs

  currentIndication.priority = LedsPriority::Alert;
  currentIndication.endTime = millis() + durationMs;
  currentIndication.drawFunc = ledsDrawBreath;
  currentIndication.solidColor = 0;
}

void ledsShowWebActivity(uint32_t color, uint32_t durationMs) {
  if (currentIndication.priority > LedsPriority::Web) return;

  WebQueuedIndication q{};
  q.breath = false;
  q.custom = false;
  q.color = color;
  q.periodMs = 0;
  q.durationMs = durationMs;
  q.drawFunc = nullptr;

  if (currentIndication.priority == LedsPriority::Status) {
    webStartFromQueued(q);
  } else {
    webQueuePush(q);
  }
}

void ledsShowWebCustom(LedsDrawFunc drawFunc, uint32_t durationMs) {
  if (currentIndication.priority > LedsPriority::Web) return;

  WebQueuedIndication q{};
  q.breath = false;
  q.custom = true;
  q.color = 0;
  q.periodMs = 0;
  q.durationMs = durationMs;
  q.drawFunc = drawFunc;

  if (currentIndication.priority == LedsPriority::Status) {
    webStartFromQueued(q);
  } else {
    webQueuePush(q);
  }
}

void ledsShowWebBreath(uint32_t color, uint32_t periodMs, uint32_t durationMs) {
  if (currentIndication.priority > LedsPriority::Web) return;

  WebQueuedIndication q{};
  q.breath = true;
  q.custom = false;
  q.color = color;
  q.periodMs = periodMs;
  q.durationMs = durationMs;
  q.drawFunc = nullptr;

  if (currentIndication.priority == LedsPriority::Status) {
    webStartFromQueued(q);
  } else {
    webQueuePush(q);
  }
}

void ledsShowCustom(LedsDrawFunc drawFunc, uint32_t durationMs, LedsPriority priority) {
  if (currentIndication.priority > priority) return;

  if (priority == LedsPriority::Alert) {
    webQueueClear();
    currentIndication.priority = LedsPriority::Alert;
    currentIndication.endTime = (durationMs > 0) ? (millis() + durationMs) : 0;
    currentIndication.drawFunc = drawFunc;
    currentIndication.solidColor = 0;
    return;
  }

  if (priority == LedsPriority::Web) {
    WebQueuedIndication q{};
    q.breath = false;
    q.custom = true;
    q.color = 0;
    q.periodMs = 0;
    q.durationMs = durationMs;
    q.drawFunc = drawFunc;
    if (currentIndication.priority == LedsPriority::Status) {
      webStartFromQueued(q);
    } else {
      webQueuePush(q);
    }
    return;
  }

  currentIndication.priority = priority;
  currentIndication.endTime = (durationMs > 0) ? (millis() + durationMs) : 0;
  currentIndication.drawFunc = drawFunc;
  currentIndication.solidColor = 0;
}

void ledsResetToStatus() {
  webQueueClear();
  currentIndication.priority = LedsPriority::Status;
  currentIndication.endTime = 0;
  currentIndication.drawFunc = nullptr;
  currentIndication.solidColor = 0;
}
