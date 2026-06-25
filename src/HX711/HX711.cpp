#include "HX711.h"
#include <string.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_SCALE
#define LOG_TAG "SCALE"
#include "debug_log.h"
#include "menu/menu_state.h"

#define CALIBRATION_MASS 1000

float calibration_mass = CALIBRATION_MASS;

static inline uint16_t current_tare(uint8_t idx) {
  switch (idx) {
  case 0:
    return static_cast<uint16_t>(menu.tare_spool1);
  case 1:
    return static_cast<uint16_t>(menu.tare_spool2);
  case 2:
    return static_cast<uint16_t>(menu.tare_spool3);
  case 3:
    return static_cast<uint16_t>(menu.tare_spool4);
  default:
    return 0;
  }
}

HX711Multi::HX711Multi(uint8_t numSensors, uint8_t dtPin, uint8_t sckPin, uint8_t aPin, uint8_t bPin) {
  // Serial.begin(57600);
  _numSensors = numSensors;
  _dtPin = dtPin;
  _sckPin = sckPin;
  _aPin = aPin;
  _bPin = bPin;
  _multiplexerPinSetFlag = false;
  sensorNum = 0;
  _prevNum = _numSensors + 1;
  memset(mass_filter, 0, sizeof(mass_filter));

  DEBUG_W("HX711Multi init: sensors=%u, DT=%u, SCK=%u, A=%u, B=%u, prev=%u", _numSensors, _dtPin, _sckPin, _aPin, _bPin, _prevNum);

  pinMode(sckPin, OUTPUT);
  pinMode(dtPin, INPUT);
  pinMode(aPin, OUTPUT);
  pinMode(bPin, OUTPUT);

  // setGain(128);

  // for (int i = 0; i < _numSensors; i++)
  // {
  //   // _sensors[i] = HX711(_dtPins[i], _sckPin);
  //   zero_weight[i] = getZeroWeight(i);
  //   offset[i] = getOffset(i);
  // }
}

void HX711Multi::begin(byte gain) {
  digitalWrite(_sckPin, HIGH);
  digitalWrite(_dtPin, LOW);
  digitalWrite(_aPin, LOW);
  digitalWrite(_bPin, LOW);
  _readStage = ReadStage::SelectMux;
  setGain(gain);
  DEBUG_T("HX711Multi::begin");
}

void HX711Multi::setNumSensors(uint8_t numSensors) {
  _numSensors = numSensors;
  _prevNum = _numSensors + 1;
  _readStage = ReadStage::SelectMux;
}

void HX711Multi::setGain(byte gain) {
  switch (gain) {
  case 128:
    GAIN = 1;
    break;
  case 64:
    GAIN = 3;
    break;
  case 32:
    GAIN = 2;
    break;
  }
  digitalWrite(_sckPin, LOW);
  readMulti(0);
}

void HX711Multi::setupGainMulti(uint8_t sensorNum) {
  for (int i = 0; i < GAIN; i++) {
    digitalWrite(_sckPin, HIGH);
    digitalWrite(_sckPin, LOW);
  }
}

// int16_t HX711Multi::readMassMulti(uint8_t scales_count)
bool HX711Multi::readMassMulti(uint8_t scales_count) {
  if (scales_count == 0) return false;
  if (scales_count != _numSensors && scales_count <= MAX_SENSORS) {
    setNumSensors(scales_count);
    sensorNum %= _numSensors;
  }

  // У переключения MUX и готовности HX711 разные условия ожидания:
  // сначала выдерживаем паузу после смены канала,
  // потом ждем готовность нового измерения от HX711.
  switch (_readStage) {
  case ReadStage::SelectMux:
    if (multiplexerPinSet(sensorNum)) {
      _readStage = ReadStage::WaitMuxSettle;
    } else {
      // На том же канале можно сразу ждать следующее готовое значение HX711.
      _readStage = ReadStage::WaitSampleReady;
    }
    return false;

  case ReadStage::WaitMuxSettle:
    if (millis() - _lastTime < RATE_DELAY) return false;
    _readStage = ReadStage::WaitDiscardReady;
    return false;

  case ReadStage::WaitDiscardReady:
    if (!readyToSend(sensorNum)) return false;

    // Первое чтение после смены канала считаем переходным:
    // выбрасываем его и этим же чтением запускаем новую конверсию HX711.
    readMulti(sensorNum);
    _readStage = ReadStage::WaitSampleReady;
    return false;

  case ReadStage::WaitSampleReady:
    if (!readyToSend(sensorNum)) return false;
    break;
  }

  const uint8_t current_sensor = sensorNum;
  int32_t raw = readMulti(current_sensor);
  _lastTimeReadMassMulti = millis();

  // 0x800000/0x7FFFFF = DT флоатит (нет чипа). Реальный чип даёт другое значение.
  if (!_scalesAvailable && raw != (int32_t)0x800000 && raw != (int32_t)0x7FFFFF) {
    _scalesAvailable = true;
    DEBUG_W("HX711 chip detected (raw=0x%06lX)", (uint32_t)raw);
  }

  for (uint8_t i = 4; i > 0; i--)
    mass_filter[current_sensor][i] = mass_filter[current_sensor][i - 1];

  uint32_t zero_weight_eep;
  uint32_t offset_eep;

  // int32_t raw = 100;

  ee_read(EE_SYS_CAL_ZERO(current_sensor), zero_weight_eep);
  ee_read(EE_SYS_CAL_OFFSET(current_sensor), offset_eep);
  DEBUG_T("sensorNum:%d\traw:%lu\tzero_weight_eep:%lu\toffset_eep:%lu", current_sensor, raw, zero_weight_eep, offset_eep);

  float grams_f = 0.0f;

  if ((int32_t)offset_eep != (int32_t)zero_weight_eep) {
    int32_t num = (int32_t)raw - (int32_t)zero_weight_eep;
    int32_t den = (int32_t)offset_eep - (int32_t)zero_weight_eep;
    // if (den == 0) return false;         // защита от деления на 0
    // if (den == 0) den = 1;              // защита от деления на 0
    grams_f = (float)calibration_mass * ((float)num / (float)den);
    if (grams_f < 0) grams_f = 0.0f; // при пустых весах не уходим в минус
  } else {
    grams_f = -1.0f;
  }

  DEBUG_T("[medsort][s%u] before: [%.2f,%.2f,%.2f,%.2f,%.2f]", current_sensor, mass_filter[current_sensor][0], mass_filter[current_sensor][1], mass_filter[current_sensor][2], mass_filter[current_sensor][3], mass_filter[current_sensor][4]);

  for (uint8_t i = 0; i < 5; i++) {
    for (uint8_t j = i + 1; j < 5; j++) {
      if (mass_filter[current_sensor][i] > mass_filter[current_sensor][j]) {
        DEBUG_T("[medsort][s%u] swap i=%u(%.2f) <-> j=%u(%.2f)", current_sensor, i, mass_filter[current_sensor][i], j, mass_filter[current_sensor][j]);

        float temp = mass_filter[current_sensor][i];
        mass_filter[current_sensor][i] = mass_filter[current_sensor][j];
        mass_filter[current_sensor][j] = temp;

        DEBUG_T("[medsort][s%u] state: [%.2f,%.2f,%.2f,%.2f,%.2f]", current_sensor, mass_filter[current_sensor][0], mass_filter[current_sensor][1], mass_filter[current_sensor][2], mass_filter[current_sensor][3], mass_filter[current_sensor][4]);
      }
    }
  }

  // mass_filter[current_sensor][0] = (grams_f + mass_filter[current_sensor][0]) / 2.0f;
  mass_filter[current_sensor][0] = grams_f;

  // после сортировки финал и медиана
  DEBUG_T("[medsort][s%u] after:  [%.2f,%.2f,%.2f,%.2f,%.2f]  median=%d", current_sensor, mass_filter[current_sensor][0], mass_filter[current_sensor][1], mass_filter[current_sensor][2], mass_filter[current_sensor][3], mass_filter[current_sensor][4], mass_filter[current_sensor][2]);

  DEBUG_T("sensorNum:%d\tmass_filter:%8.2f\tMass:%8.2f:\t\ttare:%8u:\t\tzero_weight_eep:%lu\toffset_eep:%lu", current_sensor, mass_filter[current_sensor][0], getMassMulti(current_sensor), current_tare(current_sensor), zero_weight_eep, offset_eep);

  sensorNum = (current_sensor + 1) % _numSensors;
  _readStage = ReadStage::SelectMux;
  return true;
}

float HX711Multi::getMassMulti(uint8_t sensorNum) { return mass_filter[sensorNum][2] - (float)current_tare(sensorNum); }

// Брутто: полный вес на платформе (нетто + тара держателя).
// Использовать для детекта снятия катушки — если брутто < порога,
// значит держатель убран и катушка точно снята.
float HX711Multi::getMassBruttoMulti(uint8_t sensorNum) { return mass_filter[sensorNum][2]; }

int32_t HX711Multi::readMulti(uint8_t sensorNum) {
  while (!readyToSend(sensorNum))
    watchdog_update();
  // if(!readyToSend(sensorNum)) return false;

  byte data[3];

  for (byte j = 3; j--;) {
    data[j] = shiftIn(_dtPin, _sckPin, MSBFIRST);
  }
  DEBUG_T("Raw bytes: [0]=0x%02X [1]=0x%02X [2]=0x%02X", data[0], data[1], data[2]);
  setupGainMulti(sensorNum);

  data[2] ^= 0x80;
  return ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[0];
  // }
  // else
  // {
  //   return 0;
  // }
}

bool HX711Multi::readyToSend(uint8_t sensorNum) {
  // if (multiplexerPinSet(sensorNum)) return digitalRead(_dtPin) == LOW;
  return digitalRead(_dtPin) == LOW;
}

bool HX711Multi::multiplexerPinSet(uint8_t sensorNum) {
  if (_prevNum != sensorNum) {
    digitalWrite(_aPin, (sensorNum >> 0) & 1);
    digitalWrite(_bPin, (sensorNum >> 1) & 1);
    _prevNum = sensorNum;
    _lastTime = millis();
    return true;
  }
  return false;
}

void HX711Multi::zeroSetupMulti(uint8_t sensorNum) {
  multiplexerPinSet(sensorNum);

  // Красное быстрое дыхание — "уберите руки"
  ledsShowAlertBreath(LedColors::RED, 200, 1000); // красный, 300ms период, 500ms duration
  uint32_t start = millis();
  while (millis() - start < 1000) {
    ledsTick(millis());
    watchdog_update();
    delay(10);
  }

  // После переключения канала выбрасываем первый сэмпл:
  // он может относиться к предыдущему состоянию HX711/MUX.
  readMulti(sensorNum);
  zeroSetMulti(PROBE_QUANTITY, sensorNum);
}

void HX711Multi::offsetSetupMulti(uint8_t sensorNum) {
  multiplexerPinSet(sensorNum);

  // Красное быстрое дыхание — "уберите руки"
  ledsShowAlertBreath(LedColors::RED, 200, 1000); // красный, 300ms период, 500ms duration
  uint32_t start = millis();
  while (millis() - start < 1000) {
    ledsTick(millis());
    watchdog_update();
    delay(10);
  }

  // Первый сэмпл после смены канала считаем переходным и не усредняем.
  readMulti(sensorNum);
  offsetFirstSetMulti(PROBE_QUANTITY, sensorNum);
}

void HX711Multi::offsetFirstSetMulti(uint8_t avg_size, uint8_t sensorNum) {
  // Оранжевое медленное дыхание — процесс калибровки
  ledsShowAlertBreath(LedColors::YELLOW, 3000, 25000);
  ledsTick(millis());

  while (!readyToSend(sensorNum)) {
    ledsTick(millis());
    watchdog_update();
  }

  int32_t offset = 0;

  for (int i = 0; i < int(avg_size); i++) {
    // Неблокирующая задержка с анимацией LED
    uint32_t start = millis();
    while (millis() - start < RATE_DELAY) {
      ledsTick(millis());
      watchdog_update();
      delay(1);
    }
    offset += readMulti(sensorNum);
    ledsTick(millis()); // обновляем LED после каждого замера
    DEBUG_E("[offsetFirstSetMulti]\ti%d\ttime:%lu\toffset:%lu", i, millis(), offset);
  }
  offset /= int32_t(avg_size);

  ee_write(EE_SYS_CAL_OFFSET(sensorNum), offset);
  DEBUG_T("[offsetFirstSetMulti] offset:%lu", offset);
}

void HX711Multi::tempOffsetSetMulti(uint8_t sensorNum, uint8_t temp, uint8_t avg_size) {
  // Оранжевое медленное дыхание — процесс калибровки
  ledsShowAlertBreath(LedColors::YELLOW, 3000, 25000);
  ledsTick(millis());

  while (!readyToSend(sensorNum)) {
    ledsTick(millis());
    watchdog_update();
  }

  int32_t offset = 0;

  for (int i = 0; i < int(avg_size); i++) {
    // Неблокирующая задержка с анимацией LED
    uint32_t start = millis();
    while (millis() - start < RATE_DELAY) {
      ledsTick(millis());
      watchdog_update();
      delay(1);
    }
    offset += readMulti(sensorNum);
    ledsTick(millis());
    DEBUG_W("[zeroSetMulti][%d]\ti%d\ttime:%lu\toffset:%lu", sensorNum, i, millis(), offset);
  }
  offset /= int32_t(avg_size);

  ee_write(EE_SCALE_TEMP(sensorNum, temp), offset);
  DEBUG_E("sensorNum%d\ttempOffsetSetMulti:%lu", sensorNum, offset);
}

void HX711Multi::zeroSetMulti(uint8_t avg_size, uint8_t sensorNum) {
  // Оранжевое медленное дыхание — процесс калибровки
  ledsShowAlertBreath(LedColors::YELLOW, 3000, 25000);
  ledsTick(millis());

  while (!readyToSend(sensorNum)) {
    ledsTick(millis());
    watchdog_update();
  }

  int32_t zero_weight = 0;

  for (int i = 0; i < int(avg_size); i++) {
    // Неблокирующая задержка с анимацией LED
    uint32_t start = millis();
    while (millis() - start < RATE_DELAY) {
      ledsTick(millis());
      watchdog_update();
      delay(1);
    }
    zero_weight += readMulti(sensorNum);
    ledsTick(millis()); // обновляем LED после каждого замера
    DEBUG_W("[zeroSetMulti][%d]\ti%d\ttime:%lu\tzero_weight:%lu", sensorNum, i, millis(), zero_weight);
  }
  zero_weight /= int32_t(avg_size);

  ee_write(EE_SYS_CAL_ZERO(sensorNum), zero_weight);
  DEBUG_E("sensorNum%d\tzeroSetMulti:%lu", sensorNum, zero_weight);
}


HX711::HX711(byte output_pin, byte clock_pin) {
  CLOCK_PIN = clock_pin;
  OUT_PIN = output_pin;
  GAIN = 1;
  pinsConfigured = false;
  zero_weight = 0L;
  offset = 0L;
  tare = 0;
  // ratio = 0.0;
  mass = 0;
}

HX711::~HX711() {}

bool HX711::readyToSend() {
  if (!pinsConfigured) {
    // We need to set the pin mode once, but not in the constructor
    pinMode(CLOCK_PIN, OUTPUT);
    pinMode(OUT_PIN, INPUT);
    pinsConfigured = true;
  }
  return digitalRead(OUT_PIN) == LOW;
}

void HX711::setGain(byte gain) {
  switch (gain) {
  case 128:
    GAIN = 1;
    break;
  case 64:
    GAIN = 3;
    break;
  case 32:
    GAIN = 2;
    break;
  }

  digitalWrite(CLOCK_PIN, LOW);
  read();
}

void HX711::zeroSet(uint8_t avg_size) {
  zero_weight = 0;
  for (int i = 0; i < int(avg_size); i++) {
    watchdog_update();
    delay(10);
    zero_weight += read();
  }
  zero_weight /= int32_t(avg_size);
}

int32_t HX711::zeroGet() { return zero_weight; }

void HX711::tareSet(uint8_t avg_size) {
  for (int i = 0; i < int(avg_size); i++) {
    watchdog_update();
    delay(10);
    tare += read_mass();
  }
  zero_weight /= int32_t(avg_size);
}

float HX711::tareGet() { return tare; }

// void HX711::offsetFirstSet(uint8_t avg_size) {
//   // //Serial.println("Remove  Calibrated Mass");
//   watchdog_update();
//   delay(2000);
//   // Serial.println("Add Calibrated Mass 1");
//   while (true) {
//     // //Serial.print(read());
//     // //Serial.print("\tzero_weight + 10000: ");
//     // //Serial.println(zero_weight + 10000);

//     // if (read() < zero_weight + 10000)
//     // {
//     // }
//     // else
//     // {
//     // delay(2000);
//     for (int i = 0; i < int(avg_size); i++) {
//       watchdog_update();
//       offset += read();
//     }
//     offset /= int32_t(avg_size);
//     break;
//     // }
//   }
//   // ratio = ((float)(reading - zero_weight) / (float)(offset - zero_weight));
//   // //Serial.println("Calibration Complete");
// }

void HX711::offsetSet(int32_t offset_settings) { offset = offset_settings; }

int32_t HX711::offsetGet() { return offset; }

void HX711::setupGain() {
  for (int i = 0; i < GAIN; i++) {
    digitalWrite(CLOCK_PIN, HIGH);
    digitalWrite(CLOCK_PIN, LOW);
  }
}

int32_t HX711::read() {
  while (!readyToSend())
    ;

  byte data[3];

  for (byte j = 3; j--;) {
    data[j] = shiftIn(OUT_PIN, CLOCK_PIN, MSBFIRST);
  }

  setupGain();

  data[2] ^= 0x80;
  return ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[0];
}

// uint16_t HX711::median(uint16_t newValue)
// {
//   for (uint8_t i = 4; i > 0; i--)
//   {
//     mass_filter[i] = mass_filter[i - 1];
//   }
//   mass_filter[0] = newValue;
//   for (uint8_t i = 0; i < 5; i++)
//   {
//     for (uint8_t j = i + 1; j < 5; j++)
//     {
//       if (mass_filter[i] > mass_filter[j])
//       {
//         uint16_t temp = mass_filter[i];
//         mass_filter[i] = mass_filter[j];
//         mass_filter[j] = temp;
//       }
//     }
//   }
//   return mass_filter[2];
// }

// // uint16_t HX711::median(uint16_t newValue)
// // {
// //   for (int i = 4; i > 0; i--)
// //   {
// //     mass_filter[i] = mass_filter[i - 1];
// //   }
// //   mass_filter[0] = newValue;
// //   // Create an array of indices
// //   uint16_t indices[] = {0, 1, 2, 3, 4};
// //   // Sort the indices based on the values in mass_filter
// //   for (uint8_t i = 0; i < 5; i++)
// //   {
// //     for (uint8_t j = i + 1; j < 5; j++)
// //     {
// //       if (mass_filter[indices[i]] > mass_filter[indices[j]])
// //       {
// //         uint8_t temp = indices[i];
// //         indices[i] = indices[j];
// //         indices[j] = temp;
// //       }
// //     }
// //   }
// //   return mass_filter[indices[2]];
// // }

// uint16_t HX711::read_mass()
// {
//   // mass += calibration_mass * ((float)(read() - zero_weight) / (float)(offset - zero_weight));
//   // mass /= 2.0;
//   // mass = abs(mass) < 1 ? 0 : (mass * 2 + 0.5) / 2.0 - tare;
//   // return mass;
//   mass = uint16_t(calibration_mass * ((float)(read() - zero_weight) / (float)(offset - zero_weight)));
//   uint16_t medianValue = median(mass);
//   mass = abs(medianValue) < 1 ? 0 : (medianValue * 2 + 0.5) / 2.0 - tare;
//   return mass;
// }

int16_t HX711::read_mass() {
  // Чтение значения
  mass += int16_t(calibration_mass * ((float)(read() - zero_weight) / (float)(offset - zero_weight)));
  mass /= 2;

  // Обновление медианного фильтра
  for (uint8_t i = 4; i > 0; i--)
    mass_filter[i] = mass_filter[i - 1];

  mass_filter[0] = mass;

  for (uint8_t i = 0; i < 5; i++) {
    for (uint8_t j = i + 1; j < 5; j++) {
      if (mass_filter[i] > mass_filter[j]) {
        int16_t temp = mass_filter[i];
        mass_filter[i] = mass_filter[j];
        mass_filter[j] = temp;
      }
    }
  }

  // uint16_t medianValue = mass_filter[2];
  // mass = abs(medianValue) < 1 ? 0 : (medianValue * 2 + 0.5) / 2.0 - tare;

  return mass_filter[2];
}
