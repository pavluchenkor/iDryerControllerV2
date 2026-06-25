#include "Pn5180Bus.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_TAG "PN5180BUS"
#include <debug_log.h>

namespace {
void logFramePreview(const char *prefix, const uint8_t *data, size_t len) {
  if (!data || len == 0) return;

  char line[96] = {};
  const size_t previewLen = len < 8 ? len : 8;
  size_t off = 0;
  off += snprintf(line + off, sizeof(line) - off, "%s len=%u data=",
                  prefix, static_cast<unsigned>(len));
  for (size_t i = 0; i < previewLen && off + 4 < sizeof(line); i++) {
    off += snprintf(line + off, sizeof(line) - off, "%s%02X",
                    i ? ":" : "", data[i]);
  }
  if (previewLen < len && off + 4 < sizeof(line)) {
    snprintf(line + off, sizeof(line) - off, "...");
  }
  DEBUG_D("%s", line);
}
} // namespace

Pn5180Bus::Pn5180Bus(SPIClassRP2040 &spi, uint8_t nssPin, uint8_t busyPin,
                     uint8_t rstPin, uint8_t misoPin, uint8_t mosiPin,
                     uint8_t sckPin)
    : _spi(spi), _nssPin(nssPin), _busyPin(busyPin), _rstPin(rstPin),
      _misoPin(misoPin), _mosiPin(mosiPin), _sckPin(sckPin),
      _settings(pn5180::kSpiClockHz, MSBFIRST, SPI_MODE0) {}

bool Pn5180Bus::begin() {
  pinMode(_nssPin, OUTPUT);
  digitalWrite(_nssPin, HIGH);

  pinMode(_busyPin, INPUT);
  pinMode(_rstPin, OUTPUT);
  digitalWrite(_rstPin, HIGH);

  if (!_spi.setRX(_misoPin) || !_spi.setTX(_mosiPin) || !_spi.setSCK(_sckPin)) {
    DEBUG_E("invalid SPI pin mapping rx=%u tx=%u sck=%u",
            _misoPin, _mosiPin, _sckPin);
    return false;
  }

  _spi.begin();
  DEBUG_I("SPI ready nss=%u busy=%u rst=%u rx=%u tx=%u sck=%u",
          _nssPin, _busyPin, _rstPin, _misoPin, _mosiPin, _sckPin);
  return true;
}

void Pn5180Bus::hardReset(uint32_t lowMs, uint32_t settleMs) {
  DEBUG_I("hardReset low=%lums settle=%lums",
          static_cast<unsigned long>(lowMs),
          static_cast<unsigned long>(settleMs));
  digitalWrite(_rstPin, LOW);
  delay(lowMs);
  digitalWrite(_rstPin, HIGH);
  delay(settleMs);
}

pn5180::Status Pn5180Bus::waitForBusyLevel(bool levelHigh, uint32_t timeoutMs,
                                           const char *phase) const {
  const uint32_t startedAt = millis();
  const int targetLevel = levelHigh ? HIGH : LOW;
  while (digitalRead(_busyPin) != targetLevel) {
    if (millis() - startedAt >= timeoutMs) {
      DEBUG_W("BUSY %s timeout pin=%u waited=%lums target=%s",
              phase ? phase : "state",
              _busyPin,
              static_cast<unsigned long>(millis() - startedAt),
              levelHigh ? "HIGH" : "LOW");
      return pn5180::Status::BusyTimeout;
    }
    delay(1);
  }
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180Bus::waitUntilReady(uint32_t timeoutMs) const {
  return waitForBusyLevel(false, timeoutMs, "ready");
}

pn5180::Status Pn5180Bus::waitForBusyHigh(uint32_t timeoutMs) const {
  return waitForBusyLevel(true, timeoutMs, "busy-high");
}

pn5180::Status Pn5180Bus::writeFrame(const uint8_t *data, size_t len,
                                     uint32_t timeoutMs) {
  if (!data || len == 0) {
    DEBUG_W("writeFrame invalid args len=%u", static_cast<unsigned>(len));
    return pn5180::Status::InvalidArg;
  }

  pn5180::Status st = waitUntilReady(timeoutMs);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("writeFrame wait-before failed st=%u", static_cast<unsigned>(st));
    return st;
  }

  logFramePreview("tx", data, len);

  _spi.beginTransaction(_settings);
  select();
  for (size_t i = 0; i < len; i++) {
    _spi.transfer(data[i]);
  }
  deselect();
  _spi.endTransaction();

  st = waitUntilReady(timeoutMs);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("writeFrame wait-after failed st=%u", static_cast<unsigned>(st));
  }
  return st;
}

pn5180::Status Pn5180Bus::readFrame(uint8_t *data, size_t len,
                                    uint32_t timeoutMs) {
  if (!data || len == 0) {
    DEBUG_W("readFrame invalid args len=%u", static_cast<unsigned>(len));
    return pn5180::Status::InvalidArg;
  }

  pn5180::Status st = waitUntilReady(timeoutMs);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("readFrame wait-before failed st=%u", static_cast<unsigned>(st));
    return st;
  }

  _spi.beginTransaction(_settings);
  select();
  for (size_t i = 0; i < len; i++) {
    data[i] = _spi.transfer(0x00);
  }
  deselect();
  _spi.endTransaction();

  st = waitUntilReady(timeoutMs);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("readFrame wait-after failed st=%u", static_cast<unsigned>(st));
    return st;
  }

  logFramePreview("rx", data, len);
  return st;
}

void Pn5180Bus::select() {
  digitalWrite(_nssPin, LOW);
}

void Pn5180Bus::deselect() {
  digitalWrite(_nssPin, HIGH);
}
