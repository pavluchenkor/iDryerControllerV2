#pragma once

#include <stdint.h>

// Константы цветов для LED (формат 0xRRGGBB)
namespace LedColors {
  constexpr uint32_t RED     = 0xFF0000;
  constexpr uint32_t GREEN   = 0x00FF00;
  constexpr uint32_t BLUE    = 0x0000FF;
  constexpr uint32_t YELLOW  = 0xFFFF00;
  constexpr uint32_t ORANGE  = 0xFF8000;
  constexpr uint32_t PURPLE  = 0xFF00FF;
  constexpr uint32_t CYAN    = 0x00FFFF;
  constexpr uint32_t WHITE   = 0xFFFFFF;
  constexpr uint32_t BLACK   = 0x000000;
}

// Приоритеты индикаций
enum class LedsPriority : uint8_t {
  Status = 0,  // Обычный статус периферии
  Web = 1,     // Веб-запросы/активность
  Alert = 2    // Алерты/ошибки
};

// Тип функции-коллбека для отрисовки кастомной индикации
// Параметры: now (millis), strip (Adafruit_NeoPixel*)
typedef void (*LedsDrawFunc)(uint32_t now, void* strip);

// Инициализация LED-ленты
void ledsBegin();

// Основной тик (внутри использует LedsManager)
void ledsTick(uint32_t now);

// Радужная индикация (для теста/демо)
void ledsTickRainbow(uint32_t now);

// === API LedsManager ===

// Показать алерт (приоритет Alert)
void ledsShowAlert(uint32_t color, uint32_t durationMs);
void ledsShowAlertCustom(LedsDrawFunc drawFunc, uint32_t durationMs);
void ledsShowAlertBreath(uint32_t color, uint32_t periodMs, uint32_t durationMs);

// Показать веб-активность (приоритет Web; при занятом Web — FIFO до 4 слотов)
void ledsShowWebActivity(uint32_t color, uint32_t durationMs);
void ledsShowWebCustom(LedsDrawFunc drawFunc, uint32_t durationMs);
void ledsShowWebBreath(uint32_t color, uint32_t periodMs, uint32_t durationMs);

// Показать произвольную индикацию с заданным приоритетом
void ledsShowCustom(LedsDrawFunc drawFunc, uint32_t durationMs, LedsPriority priority);

// Принудительно сбросить текущую индикацию к статусу периферии
void ledsResetToStatus();
