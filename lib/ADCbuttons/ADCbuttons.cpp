#include "ADCbuttons.h"

// Простая abs для int (чтобы не зависеть от stdlib.h)
static inline int abs_int(int x) { return x < 0 ? -x : x; }

ADCbuttons::ADCbuttons(uint8_t pin, uint16_t noPress, uint16_t tol) : _pin(pin), noPressValue(noPress), tolerance(tol) { pinMode(_pin, INPUT); }

void ADCbuttons::addButton(uint16_t adcValue, uint8_t id) {
  if (btnCount < MAX_BTNS) {
    buttons[btnCount].val = adcValue;
    buttons[btnCount].id = id;
    btnCount++;
  }
}

// Старый метод — оставим как "моментальный" опрос без фронтов
uint8_t ADCbuttons::readButton() { return readInstantId(); }

// Приватный быстрый маппер ADC -> ID
uint8_t ADCbuttons::readInstantId() {
  uint16_t val = analogRead(_pin);

  // Ничего не нажато
  if (abs_int((int)val - (int)noPressValue) <= (int)tolerance) {
    return 0;
  }

  // Ищем ближайшую кнопку по допустимому отклонению
  for (uint8_t i = 0; i < btnCount; i++) {
    if (abs_int((int)val - (int)buttons[i].val) <= (int)tolerance) {
      return buttons[i].id;
    }
  }
  return 0;
}

// Неблокирующий тикер с антидребезгом + генерация фронтов
void ADCbuttons::tick(uint32_t now) {
  uint8_t instantId = readInstantId();

  // Дебаунс: фиксируем смену, только если состояние стабильно >= DEBOUNCE_MS
  if (instantId != _stableId) {
    _stableId = instantId;
    _lastChangeMs = now; // началась «возможная» смена
    return;              // ждём стабилизации
  }

  // Если стабильно и прошло достаточно времени — принимаем как текущее
  if (_currId != _stableId && (now - _lastChangeMs) >= DEBOUNCE_MS) {
    _lastId = _currId;
    _currId = _stableId;

    // Генерируем фронты
    if (_lastId == 0 && _currId != 0) {
      _downAt = now;
      _edgePressId = _currId; // одноразовый флаг на press

      // === НОВЫЙ API: создаём/сбрасываем ButtonState при нажатии ===
      int8_t idx = findStateIndex(_currId);
      if (idx < 0) {
        // Создаём новый ButtonState
        for (uint8_t i = 0; i < MAX_BTNS; i++) {
          if (_btnStates[i].id == 0) {
            _btnStates[i].id = _currId;
            _btnStates[i].pressedAt = now;
            _btnStates[i].holdFired = false;
            _btnStates[i].longHoldFired = false;
            _btnStates[i].pendingEvent = ButtonEvent::None;
            idx = i;
            break;
          }
        }
      } else {
        // Сбрасываем существующий
        _btnStates[idx].pressedAt = now;
        _btnStates[idx].holdFired = false;
        _btnStates[idx].longHoldFired = false;
        _btnStates[idx].pendingEvent = ButtonEvent::None;
      }

    } else if (_lastId != 0 && _currId == 0) {
      _upAt = now;
      _edgeReleaseId = _lastId; // одноразовый флаг на release (КЛИК)

      // === НОВЫЙ API: генерируем Click если не было удержаний ===
      int8_t idx = findStateIndex(_lastId);
      if (idx >= 0) {
        if (!_btnStates[idx].holdFired && !_btnStates[idx].longHoldFired) {
          _btnStates[idx].pendingEvent = ButtonEvent::Click;
        }
        // НЕ очищаем ID здесь - будет очищено в getEvent() после чтения события
      }
    }
  }

  // === НОВЫЙ API: проверка удержаний для активной кнопки ===
  if (_currId != 0) {
    int8_t idx = findStateIndex(_currId);
    if (idx >= 0) {
      uint32_t holdDuration = now - _btnStates[idx].pressedAt;

      // Проверка долгого удержания
      if (!_btnStates[idx].longHoldFired && holdDuration >= _longHoldMs) {
        _btnStates[idx].pendingEvent = ButtonEvent::LongHold;
        _btnStates[idx].longHoldFired = true;
        _btnStates[idx].holdFired = true; // чтобы Hold не сработал
      }
      // Проверка обычного удержания
      else if (!_btnStates[idx].holdFired && holdDuration >= _holdMs) {
        _btnStates[idx].pendingEvent = ButtonEvent::Hold;
        _btnStates[idx].holdFired = true;
      }
    }
  }
}

uint8_t ADCbuttons::justPressed() {
  uint8_t id = _edgePressId;
  _edgePressId = 0;
  return id;
}

uint8_t ADCbuttons::justReleased() {
  uint8_t id = _edgeReleaseId;
  _edgeReleaseId = 0;
  return id;
}

// === API ===

void ADCbuttons::setHoldDuration(uint16_t holdMs, uint16_t longHoldMs) {
  _holdMs = holdMs;
  _longHoldMs = longHoldMs;
}

int8_t ADCbuttons::findStateIndex(uint8_t id) {
  for (uint8_t i = 0; i < MAX_BTNS; i++) {
    if (_btnStates[i].id == id) return i;
  }
  return -1;
}

ButtonEvent ADCbuttons::getEvent(uint8_t id) {
  int8_t idx = findStateIndex(id);
  if (idx < 0) return ButtonEvent::None;

  ButtonEvent ev = _btnStates[idx].pendingEvent;
  _btnStates[idx].pendingEvent = ButtonEvent::None;

  // Очищаем состояние кнопки ПОСЛЕ чтения события (если кнопка уже отпущена)
  if (ev != ButtonEvent::None && _currId == 0) {
    _btnStates[idx].id = 0;
  }

  return ev;
}
