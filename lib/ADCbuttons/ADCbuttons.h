#pragma once
#include <Arduino.h>

// Типы событий кнопки
enum class ButtonEvent : uint8_t {
  None = 0,      // нет события
  Click,         // короткий клик (отпущена до hold)
  Hold,          // удержание (по умолчанию 1000ms)
  LongHold       // долгое удержание (по умолчанию 2000ms)
};

class ADCbuttons {
public:
  ADCbuttons(uint8_t pin, uint16_t noPress, uint16_t tol);
  void addButton(uint16_t adcValue, uint8_t id);

  // Старый способ (текущее состояние, 0 = не нажато)
  uint8_t readButton();

  // Новый неблокирующий цикл
  void tick(uint32_t now = millis());

  // Фронты (события) возвращают ID один раз на событие, иначе 0
  uint8_t justPressed();  // произошёл переход 0->ID
  uint8_t justReleased(); // произошёл переход ID->0 ("клик по отпусканию")

  // Удобные методы
  uint8_t current() const { return _currId; }
  bool held(uint32_t ms, uint32_t now = millis()) const { return _currId && (now - _downAt >= ms); }

  // === НОВЫЙ API: события по ID кнопки ===
  // Настройка длительности удержаний (по умолчанию: hold=1000ms, longHold=2000ms)
  void setHoldDuration(uint16_t holdMs, uint16_t longHoldMs);

  // Получить событие для кнопки (одноразово, потом сбрасывается в None)
  ButtonEvent getEvent(uint8_t id);

private:
  struct Btn {
    uint16_t val;
    uint8_t id;
  };

  // Состояние кнопки для отслеживания событий
  struct ButtonState {
    uint8_t id = 0;              // ID кнопки
    uint32_t pressedAt = 0;      // время нажатия
    bool holdFired = false;      // сработало ли событие Hold
    bool longHoldFired = false;  // сработало ли событие LongHold
    ButtonEvent pendingEvent = ButtonEvent::None;  // ожидающее событие
  };

  static constexpr uint8_t MAX_BTNS = 8;
  static constexpr uint16_t DEBOUNCE_MS = 30; // антидребезг

  uint8_t _pin;
  uint16_t noPressValue;
  uint16_t tolerance;

  Btn buttons[MAX_BTNS];
  uint8_t btnCount = 0;

  // Состояние
  uint8_t _stableId = 0; // «сырое» устойчивое состояние после дебаунса
  uint8_t _currId = 0;   // принятое текущее состояние
  uint8_t _lastId = 0;
  uint32_t _lastChangeMs = 0; // для дебаунса
  uint32_t _downAt = 0;       // когда нажали
  uint32_t _upAt = 0;         // когда отпустили

  // Одноразовые флаги фронтов
  uint8_t _edgePressId = 0;
  uint8_t _edgeReleaseId = 0;

  // Состояния кнопок для отслеживания событий
  ButtonState _btnStates[MAX_BTNS];
  uint16_t _holdMs = 1000;      // длительность для Hold
  uint16_t _longHoldMs = 2000;  // длительность для LongHold

  // внутреннее чтение «сырых» значений -> ID
  uint8_t readInstantId();

  // Найти индекс ButtonState по ID кнопки
  int8_t findStateIndex(uint8_t id);
};
