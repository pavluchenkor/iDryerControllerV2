#pragma once
#include <Arduino.h>
#include "controller/control.h"   // DryerMode
#include "sensor/Sensor.h"        // DryerInputs
#include "configuration.h"
#include "HX711/HX711.h"

constexpr uint32_t IDLE_TO_STATUS_MS = 15000;   //! через 15с в idle показываем статус
constexpr uint32_t MODE_SWAP_MS      = 5000;   // каждые 4с переключаемся "режим <-> весы"
constexpr uint32_t UNIT_ROTATE_MS    = 10000;  // каждые 6с перелистываем юнит
constexpr uint32_t SCREEN_OFF_MS     = 180000; // гасим экран после 3 мин без ввода

extern uint8_t      gscr_active_unit;
extern uint32_t     gscr_next_unit_rotate_ms;
extern uint32_t     g_lastInputMs;             // последний ввод (энкодер/кнопки), любой юнит

struct ScreenState {
  enum class Kind : uint8_t {Error, Menu, IdleStatus, Drying, Storage, AutoTune, Weight };

  // что сейчас хотим рисовать
  Kind kind         = Kind::Menu;

  // антизалип и авто-переключение
  uint32_t lastInputMs = 0;          // когда был последний пользовательский ввод
  bool     weightPhase = false;      // false=режим, true=весы
  uint32_t nextSwapMs  = 0;          // когда сменить фазу
};

// Один ScreenState на каждый юнит.
extern ScreenState gscr_units[NUM_UNITS];

extern HX711Multi* hx711MultiPtr;
#define hx711Multi (*hx711MultiPtr)

void markUserInput(uint8_t unit_id, uint32_t now);