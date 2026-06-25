#pragma once

#include "Pn5180Defs.h"
#include <Arduino.h>
#include <SPI.h>

class Pn5180Bus {
public:
  Pn5180Bus(SPIClassRP2040 &spi, uint8_t nssPin, uint8_t busyPin, uint8_t rstPin,
            uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin);

  bool begin();
  void hardReset(uint32_t lowMs = pn5180::kResetLowMs,
                 uint32_t settleMs = pn5180::kResetSettleMs);

  pn5180::Status waitUntilReady(uint32_t timeoutMs = pn5180::kBusyTimeoutMs) const;
  pn5180::Status writeFrame(const uint8_t *data, size_t len,
                            uint32_t timeoutMs = pn5180::kBusyTimeoutMs);
  pn5180::Status readFrame(uint8_t *data, size_t len,
                           uint32_t timeoutMs = pn5180::kBusyTimeoutMs);

  pn5180::Status waitForBusyHigh(uint32_t timeoutMs = pn5180::kBusyTimeoutMs) const;

private:
  SPIClassRP2040 &_spi;
  uint8_t _nssPin;
  uint8_t _busyPin;
  uint8_t _rstPin;
  uint8_t _misoPin;
  uint8_t _mosiPin;
  uint8_t _sckPin;
  SPISettings _settings;

  pn5180::Status waitForBusyLevel(bool levelHigh, uint32_t timeoutMs,
                                  const char *phase) const;
  void select();
  void deselect();
};
