#pragma once

#include "../GpioCs.h"
#include "../IRfidDriver.h"
#include <Pn5180.h>

class Pn5180Driver : public IRfidDriver {
public:
  Pn5180Driver(SPIClassRP2040 &spi, GpioCs &cs, uint8_t busyPin, uint8_t rstPin,
               uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin);

  bool init() override;
  RfidStatus poll(RfidTag &tag) override;
  RfidStatus readRaw(const RfidTag &tag, uint8_t startPage, uint8_t *buf,
                     size_t len) override;
  RfidStatus writeRaw(const RfidTag &tag, uint8_t startPage, const uint8_t *data,
                      size_t len) override;
  void setScanProtocols(RfidProtocolMask mask) override;

  enum class PollProtocol : uint8_t {
    Iso15693,
    TypeA,
    TypeB,
    TypeF,
  };

private:

  Pn5180 _chip;
  RfidTag _lastTag = {};
  bool _tagPresent = false;
  uint32_t _iso15693GeometryRetryAtMs = 0;
  PollProtocol _preferredProtocol = PollProtocol::Iso15693;
  PollProtocol _scanProtocol = PollProtocol::Iso15693;
  RfidProtocolMask _scanMask = RFID_PROTOCOL_ALL;

  bool _typeASelectionValid = false;
  uint8_t _typeASelectionUid[10] = {};
  uint8_t _typeASelectionUidLen = 0;
  uint32_t _typeASelectionAtMs = 0;

  // MIFARE Classic: track authenticated sector to avoid re-auth
  uint8_t _mifareAuthSector = 0xFF;  // 0xFF = none

  bool sameTag(const RfidTag &tag) const;
  bool ensureTypeATagSelected(const RfidTag &tag);
  void cacheTypeASelection(const uint8_t *uid, uint8_t uidLen);
  void invalidateTypeASelection();
  RfidStatus pollIso15693(RfidTag &tag);
  RfidStatus pollTypeA(RfidTag &tag);
  RfidStatus pollTypeB(RfidTag &tag);
  RfidStatus pollTypeF(RfidTag &tag);
};
