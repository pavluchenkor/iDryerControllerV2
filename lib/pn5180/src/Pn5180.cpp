#include "Pn5180.h"

#include <string.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_TAG "PN5180LIB"
#include <debug_log.h>

namespace {
bool shouldLogPn5180Timeout() {
  static uint32_t lastLogAtMs = 0;
  const uint32_t now = millis();
  if (now - lastLogAtMs < 1000) {
    return false;
  }
  lastLogAtMs = now;
  return true;
}

constexpr uint32_t kTypeAWriteCycleDelayMs = 8;
constexpr uint32_t kTypeAWriteAckPollDelayMs = 5;
} // namespace

Pn5180::Pn5180(SPIClassRP2040 &spi, uint8_t nssPin, uint8_t busyPin,
               uint8_t rstPin, uint8_t misoPin, uint8_t mosiPin,
               uint8_t sckPin)
    : _bus(spi, nssPin, busyPin, rstPin, misoPin, mosiPin, sckPin) {}

bool Pn5180::begin() {
  if (!_bus.begin()) return false;

  hardReset();

  uint32_t systemStatus = 0;
  pn5180::Status st = readRegister(pn5180::SYSTEM_STATUS, systemStatus);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("SYSTEM_STATUS read failed st=%u", static_cast<unsigned>(st));
    return false;
  }
  DEBUG_I("SYSTEM_STATUS=0x%08lX", static_cast<unsigned long>(systemStatus));

  // Read PN5180 identity from EEPROM (product/firmware/eeprom versions)
  // READ_EEPROM (0x07): addr=0x10, len=5 → product[2]+firmware[2]+eeprom[1]
  {
    const uint8_t readEepromCmd[] = {0x07, 0x10, 0x05};
    st = _bus.writeFrame(readEepromCmd, sizeof(readEepromCmd));
    if (st == pn5180::Status::Ok) {
      uint8_t ver[5] = {};
      st = _bus.readFrame(ver, sizeof(ver));
      if (st == pn5180::Status::Ok) {
        DEBUG_I("PN5180 product=%u.%u firmware=%u.%u eeprom=%u",
                ver[1], ver[0], ver[3], ver[2], ver[4]);
      }
    }
  }

  return true;
}

void Pn5180::hardReset() {
  _bus.hardReset();
  _rfFieldEnabled = false;

  uint32_t irqStatus = 0;
  pn5180::Status st = waitForIrq(pn5180::IRQ_IDLE, irqStatus);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("post-reset IDLE irq wait failed st=%u", static_cast<unsigned>(st));
    return;
  }
  (void)clearIrqStatus();
}

void Pn5180::recoverBusFault(const char *context) {
  if (_recovering) return;
  _recovering = true;
  DEBUG_W("hard recover at %s", context ? context : "unknown");
  _bus.hardReset();
  _rfFieldEnabled = false;
  _recovering = false;
}

pn5180::Status Pn5180::readRegister(uint8_t address, uint32_t &value) {
  const uint8_t frame[] = {
      static_cast<uint8_t>(pn5180::Command::ReadRegister),
      address,
  };
  pn5180::Status st = _bus.writeFrame(frame, sizeof(frame));
  if (st != pn5180::Status::Ok) {
    if (st == pn5180::Status::BusyTimeout) recoverBusFault("readRegister/writeFrame");
    return st;
  }

  uint8_t response[4] = {};
  st = _bus.readFrame(response, sizeof(response));
  if (st != pn5180::Status::Ok) {
    if (st == pn5180::Status::BusyTimeout) recoverBusFault("readRegister/readFrame");
    return st;
  }

  value = readLe32(response);
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::writeRegister(uint8_t address, uint32_t value) {
  uint8_t frame[6] = {
      static_cast<uint8_t>(pn5180::Command::WriteRegister),
      address,
  };
  writeLe32(frame + 2, value);
  pn5180::Status st = _bus.writeFrame(frame, sizeof(frame));
  if (st == pn5180::Status::BusyTimeout) recoverBusFault("writeRegister");
  return st;
}

pn5180::Status Pn5180::writeRegisterOrMask(uint8_t address, uint32_t mask) {
  uint8_t frame[6] = {
      static_cast<uint8_t>(pn5180::Command::WriteRegisterOrMask),
      address,
  };
  writeLe32(frame + 2, mask);
  pn5180::Status st = _bus.writeFrame(frame, sizeof(frame));
  if (st == pn5180::Status::BusyTimeout) recoverBusFault("writeRegisterOrMask");
  return st;
}

pn5180::Status Pn5180::writeRegisterAndMask(uint8_t address, uint32_t mask) {
  uint8_t frame[6] = {
      static_cast<uint8_t>(pn5180::Command::WriteRegisterAndMask),
      address,
  };
  writeLe32(frame + 2, mask);
  pn5180::Status st = _bus.writeFrame(frame, sizeof(frame));
  if (st == pn5180::Status::BusyTimeout) recoverBusFault("writeRegisterAndMask");
  return st;
}

pn5180::Status Pn5180::clearIrqStatus(uint32_t mask) {
  return writeRegister(pn5180::IRQ_CLEAR, mask);
}

pn5180::Status Pn5180::loadRfConfig(uint8_t txConfig, uint8_t rxConfig) {
  const uint8_t payload[] = {txConfig, rxConfig};
  return sendCommand(pn5180::Command::LoadRfConfig, payload, sizeof(payload));
}

pn5180::Status Pn5180::fieldOn() {
  if (_rfFieldEnabled) return pn5180::Status::Ok;

  const uint8_t payload[] = {0x00};
  pn5180::Status st = clearIrqStatus(pn5180::IRQ_TX_RFON);
  if (st != pn5180::Status::Ok) return st;

  st = sendCommand(pn5180::Command::RfOn, payload, sizeof(payload));
  if (st != pn5180::Status::Ok) return st;

  uint32_t irqStatus = 0;
  st = waitForIrq(pn5180::IRQ_TX_RFON, irqStatus);
  if (st != pn5180::Status::Ok) return st;
  st = clearIrqStatus(pn5180::IRQ_TX_RFON);
  if (st != pn5180::Status::Ok) return st;
  _rfFieldEnabled = true;
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::fieldOff() {
  if (!_rfFieldEnabled) return pn5180::Status::Ok;

  const uint8_t payload[] = {0x00};
  pn5180::Status st = clearIrqStatus(pn5180::IRQ_TX_RFOFF);
  if (st != pn5180::Status::Ok) return st;

  st = sendCommand(pn5180::Command::RfOff, payload, sizeof(payload));
  if (st != pn5180::Status::Ok) return st;

  uint32_t irqStatus = 0;
  st = waitForIrq(pn5180::IRQ_TX_RFOFF, irqStatus);
  if (st != pn5180::Status::Ok) return st;
  st = clearIrqStatus(pn5180::IRQ_TX_RFOFF);
  if (st != pn5180::Status::Ok) return st;
  _rfFieldEnabled = false;
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::setIdle() {
  return writeRegisterAndMask(pn5180::SYSTEM_CONFIG,
                              ~pn5180::SYSTEM_CONFIG_COMMAND_MASK);
}

pn5180::Status Pn5180::setTransceive() {
  return writeRegisterOrMask(pn5180::SYSTEM_CONFIG,
                             pn5180::SYSTEM_CONFIG_COMMAND_TRANSCEIVE);
}

pn5180::Status Pn5180::sendData(const uint8_t *data, size_t len,
                                uint8_t validBits) {
  if (!data || len == 0 || len > 260) return pn5180::Status::InvalidArg;

  uint8_t frame[262] = {
      static_cast<uint8_t>(pn5180::Command::SendData),
      validBits,
  };
  memcpy(frame + 2, data, len);
  pn5180::Status st = _bus.writeFrame(frame, len + 2);
  if (st == pn5180::Status::BusyTimeout) recoverBusFault("sendData");
  return st;
}

pn5180::Status Pn5180::readData(uint8_t *data, size_t len) {
  if (!data || len == 0) return pn5180::Status::InvalidArg;

  const uint8_t frame[] = {
      static_cast<uint8_t>(pn5180::Command::ReadData),
      0x00,
  };
  pn5180::Status st = _bus.writeFrame(frame, sizeof(frame));
  if (st != pn5180::Status::Ok) {
    if (st == pn5180::Status::BusyTimeout) recoverBusFault("readData/writeFrame");
    return st;
  }

  st = _bus.readFrame(data, len);
  if (st == pn5180::Status::BusyTimeout) recoverBusFault("readData/readFrame");
  return st;
}

pn5180::Status Pn5180::inventory(pn5180::InventoryResponse &response,
                                 uint32_t timeoutMs) {
  response = {};

  const uint8_t inventoryFrame[] = {
      static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH |
                           pn5180::ISO15693_FLAG_INVENTORY |
                           pn5180::ISO15693_FLAG_1_SLOT),
      pn5180::ISO15693_CMD_INVENTORY,
      0x00,
  };

  pn5180::Iso15693Response isoResponse{};
  pn5180::Status st =
      transceiveIso15693(inventoryFrame, sizeof(inventoryFrame), isoResponse, timeoutMs);
  if (st != pn5180::Status::Ok) return st;

  response.irqStatus = isoResponse.irqStatus;
  response.rxStatus = isoResponse.rxStatus;
  response.length = isoResponse.length;
  if (response.length > sizeof(response.data)) {
    response.length = sizeof(response.data);
  }
  memcpy(response.data, isoResponse.data, response.length);
  response.tagFound = response.length >= 10;
  if (response.tagFound) {
    DEBUG_D("inventory hit len=%u irq=0x%08lX rx=0x%08lX",
            static_cast<unsigned>(response.length),
            static_cast<unsigned long>(response.irqStatus),
            static_cast<unsigned long>(response.rxStatus));
  }
  return response.tagFound ? pn5180::Status::Ok : pn5180::Status::Timeout;
}

pn5180::Status Pn5180::pollTypeA(pn5180::TypeAResponse &response,
                                 uint32_t timeoutMs) {
  response = {};

  pn5180::Status st = loadTypeA106Config();
  if (st != pn5180::Status::Ok) return st;

  st = fieldOn();
  if (st != pn5180::Status::Ok) return st;

  // Clear MFC_CRYPTO_ON (bit 6) — loadRfConfig may have restored it from
  // EEPROM, and a leftover Crypto1 session encrypts WUPA making tag unreachable.
  st = writeRegisterAndMask(pn5180::SYSTEM_CONFIG, ~(1u << 6));
  if (st != pn5180::Status::Ok) return st;

  st = setCrcTx(false);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(false);
  if (st != pn5180::Status::Ok) return st;

  pn5180::Iso15693Response raw{};
  const uint8_t wupa[] = {0x52};
  st = transceiveRaw(wupa, sizeof(wupa), raw, 7, true, timeoutMs);
  if (st == pn5180::Status::Timeout) {
    const uint8_t reqa[] = {0x26};
    st = transceiveRaw(reqa, sizeof(reqa), raw, 7, true, timeoutMs);
  }
  if (st != pn5180::Status::Ok) return st;
  if (raw.length < 2) return pn5180::Status::ProtocolError;
  response.atqa[0] = raw.data[0];
  response.atqa[1] = raw.data[1];

  if (response.atqa[0] == 0x0C && response.atqa[1] == 0x00) {
    st = setCrcTx(true);
    if (st != pn5180::Status::Ok) return st;
    st = setCrcRx(true);
    if (st != pn5180::Status::Ok) return st;

    const uint8_t rid[] = {0x78, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    st = transceiveRaw(rid, sizeof(rid), raw, 0, true, timeoutMs);
    if (st != pn5180::Status::Ok) return st;
    if (raw.length < 6) return pn5180::Status::ProtocolError;

    response.isTopaz = true;
    response.hr[0] = raw.data[0];
    response.hr[1] = raw.data[1];
    memcpy(response.uid, raw.data + 2, 4);
    response.uidLen = 4;
    response.tagFound = true;
    DEBUG_D("type1/topaz hit hr=%02X%02X uid=%02X:%02X:%02X:%02X",
            response.hr[0], response.hr[1],
            response.uid[0], response.uid[1], response.uid[2], response.uid[3]);
    return pn5180::Status::Ok;
  }

  uint8_t cascadeLevel = 0;
  uint8_t uidOffset = 0;
  uint8_t selCode = 0x93;
  bool cascaded = false;
  do {
    const uint8_t anticoll[] = {selCode, 0x20};
    st = transceiveRaw(anticoll, sizeof(anticoll), raw, 0, true, timeoutMs);
    if (st != pn5180::Status::Ok) return st;
    if (raw.length < 5) return pn5180::Status::ProtocolError;

    uint8_t anticollUid[5] = {};
    memcpy(anticollUid, raw.data, sizeof(anticollUid));

    st = setCrcTx(true);
    if (st != pn5180::Status::Ok) return st;
    st = setCrcRx(true);
    if (st != pn5180::Status::Ok) return st;

    uint8_t selectFrame[7] = {selCode, 0x70, 0, 0, 0, 0, 0};
    memcpy(selectFrame + 2, anticollUid, sizeof(anticollUid));
    st = transceiveRaw(selectFrame, sizeof(selectFrame), raw, 0, true, timeoutMs);
    if (st != pn5180::Status::Ok) return st;
    if (raw.length < 1) return pn5180::Status::ProtocolError;
    response.sak = raw.data[0];

    st = setCrcTx(false);
    if (st != pn5180::Status::Ok) return st;
    st = setCrcRx(false);
    if (st != pn5180::Status::Ok) return st;

    if (anticollUid[0] == 0x88) {
      if (uidOffset + 3 > sizeof(response.uid)) return pn5180::Status::ProtocolError;
      memcpy(response.uid + uidOffset, anticollUid + 1, 3);
      uidOffset += 3;
      cascaded = (response.sak & 0x04) != 0;
      selCode = (cascadeLevel == 0) ? 0x95 : 0x97;
      cascadeLevel++;
    } else {
      if (uidOffset + 4 > sizeof(response.uid)) return pn5180::Status::ProtocolError;
      memcpy(response.uid + uidOffset, anticollUid, 4);
      uidOffset += 4;
      cascaded = (response.sak & 0x04) != 0;
      selCode = (cascadeLevel == 0) ? 0x95 : 0x97;
      cascadeLevel++;
    }
  } while (cascaded && cascadeLevel < 3);

  response.uidLen = uidOffset;
  response.tagFound = response.uidLen > 0;
  DEBUG_D("typeA hit atqa=%02X%02X sak=0x%02X uidLen=%u",
          response.atqa[0], response.atqa[1], response.sak, response.uidLen);
  return response.tagFound ? pn5180::Status::Ok : pn5180::Status::Timeout;
}

pn5180::Status Pn5180::pollTypeB(pn5180::TypeBResponse &response,
                                 uint32_t timeoutMs) {
  response = {};

  pn5180::Status st = loadTypeB106Config();
  if (st != pn5180::Status::Ok) return st;

  st = fieldOn();
  if (st != pn5180::Status::Ok) return st;

  st = setCrcTx(true);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(true);
  if (st != pn5180::Status::Ok) return st;

  pn5180::Iso15693Response raw{};
  const uint8_t reqb[] = {0x05, 0x00, 0x00};
  st = transceiveRaw(reqb, sizeof(reqb), raw, 0, true, timeoutMs);
  if (st != pn5180::Status::Ok) return st;
  if (raw.length < 11) return pn5180::Status::ProtocolError;

  memcpy(response.pupi, raw.data, sizeof(response.pupi));
  memcpy(response.appData, raw.data + 4, sizeof(response.appData));
  memcpy(response.protocolInfo, raw.data + 8, sizeof(response.protocolInfo));
  response.afi = response.appData[0];
  response.tagFound = true;
  DEBUG_D("typeB hit pupi=%02X:%02X:%02X:%02X afi=0x%02X",
          response.pupi[0], response.pupi[1], response.pupi[2], response.pupi[3],
          response.afi);
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::pollTypeF(pn5180::TypeFResponse &response,
                                 uint32_t timeoutMs) {
  response = {};

  pn5180::Status st = loadTypeF212Config();
  if (st != pn5180::Status::Ok) return st;

  st = fieldOn();
  if (st != pn5180::Status::Ok) return st;

  st = setCrcTx(false);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(false);
  if (st != pn5180::Status::Ok) return st;

  pn5180::Iso15693Response raw{};
  const uint8_t polling[] = {0x06, 0x00, 0xFF, 0xFF, 0x00, 0x00};
  st = transceiveRaw(polling, sizeof(polling), raw, 0, true, timeoutMs);
  if (st != pn5180::Status::Ok) return st;
  if (raw.length < 18) return pn5180::Status::ProtocolError;
  if (raw.data[1] != 0x01) return pn5180::Status::ProtocolError;

  memcpy(response.idm, raw.data + 2, sizeof(response.idm));
  memcpy(response.pmm, raw.data + 10, sizeof(response.pmm));
  if (raw.length >= 20) {
    response.systemCode[0] = raw.data[18];
    response.systemCode[1] = raw.data[19];
  }
  response.tagFound = true;
  DEBUG_D("typeF hit idm=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
          response.idm[0], response.idm[1], response.idm[2], response.idm[3],
          response.idm[4], response.idm[5], response.idm[6], response.idm[7]);
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::readTypeAPage(uint8_t page, uint8_t *pageData,
                                     uint32_t timeoutMs) {
  if (!pageData) return pn5180::Status::InvalidArg;

  pn5180::Status st = setCrcTx(true);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(true);
  if (st != pn5180::Status::Ok) return st;

  const uint8_t readCmd[] = {0x30, page};
  pn5180::Iso15693Response raw{};
  st = transceiveRaw(readCmd, sizeof(readCmd), raw, 0, true, timeoutMs);
  if (st != pn5180::Status::Ok) return st;
  if (raw.length < 4) return pn5180::Status::ProtocolError;

  memcpy(pageData, raw.data, 4);
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::writeTypeAPage(uint8_t page, const uint8_t *pageData,
                                      uint32_t timeoutMs) {
  if (!pageData) return pn5180::Status::InvalidArg;

  pn5180::Status st = setCrcTx(true);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(true);
  if (st != pn5180::Status::Ok) return st;

  uint8_t writeCmd[6] = {0xA2, page, 0, 0, 0, 0};
  memcpy(writeCmd + 2, pageData, 4);

  st = clearIrqStatus();
  if (st != pn5180::Status::Ok) return st;

  st = setIdle();
  if (st != pn5180::Status::Ok) return st;

  st = setTransceive();
  if (st != pn5180::Status::Ok) return st;

  st = sendData(writeCmd, sizeof(writeCmd), 0);
  if (st != pn5180::Status::Ok) return st;

  uint32_t irq = 0;
  st = waitForIrq(pn5180::IRQ_TX, irq, 10);
  if (st != pn5180::Status::Ok) {
    (void)setIdle();
    (void)clearIrqStatus();
    return st;
  }

  delay(kTypeAWriteAckPollDelayMs);

  (void)setIdle();
  (void)clearIrqStatus();
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::getSystemInfo(const uint8_t *uid, pn5180::SystemInfo &info,
                                     uint32_t timeoutMs) {
  if (!uid) return pn5180::Status::InvalidArg;

  uint8_t frame[10] = {
      static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH |
                           pn5180::ISO15693_FLAG_ADDRESS),
      pn5180::ISO15693_CMD_GET_SYSTEM_INFO,
      0, 0, 0, 0, 0, 0, 0, 0,
  };
  memcpy(frame + 2, uid, 8);

  pn5180::Iso15693Response response{};
  pn5180::Status st = transceiveIso15693(frame, sizeof(frame), response, timeoutMs);
  if (st != pn5180::Status::Ok) {
    const uint8_t fallbackFrame[] = {
        static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH),
        pn5180::ISO15693_CMD_GET_SYSTEM_INFO,
    };
    DEBUG_W("getSystemInfo addressed failed st=%u, trying non-addressed fallback",
            static_cast<unsigned>(st));
    st = transceiveIso15693(fallbackFrame, sizeof(fallbackFrame), response, timeoutMs);
    if (st != pn5180::Status::Ok) return st;
  }
  if (response.length < 10) {
    uint8_t b0 = response.length > 0 ? response.data[0] : 0;
    uint8_t b1 = response.length > 1 ? response.data[1] : 0;
    uint8_t b2 = response.length > 2 ? response.data[2] : 0;
    uint8_t b3 = response.length > 3 ? response.data[3] : 0;
    DEBUG_W("getSystemInfo short response len=%u b0=0x%02X b1=0x%02X b2=0x%02X b3=0x%02X errFlag=%u",
            static_cast<unsigned>(response.length),
            static_cast<unsigned>(b0), static_cast<unsigned>(b1),
            static_cast<unsigned>(b2), static_cast<unsigned>(b3),
            static_cast<unsigned>((b0 & 0x01) ? 1 : 0));
    return pn5180::Status::ProtocolError;
  }

  DEBUG_W("getSystemInfo raw len=%u: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
          static_cast<unsigned>(response.length),
          response.data[0], response.data[1], response.data[2], response.data[3],
          response.data[4], response.data[5], response.data[6], response.data[7],
          response.data[8], response.data[9], response.data[10], response.data[11],
          response.data[12], response.data[13], response.data[14]);

  info = {};
  size_t offset = 0;
  offset++; // response flags
  info.infoFlags = response.data[offset++];
  memcpy(info.uid, response.data + offset, sizeof(info.uid));
  offset += sizeof(info.uid);

  if (info.infoFlags & 0x01) {
    if (offset >= response.length) { DEBUG_W("getSystemInfo FAIL at DSFID offset=%u len=%u", (unsigned)offset, (unsigned)response.length); return pn5180::Status::ProtocolError; }
    info.dsfid = response.data[offset++];
  }
  if (info.infoFlags & 0x02) {
    if (offset >= response.length) { DEBUG_W("getSystemInfo FAIL at AFI offset=%u len=%u", (unsigned)offset, (unsigned)response.length); return pn5180::Status::ProtocolError; }
    info.afi = response.data[offset++];
  }
  if (info.infoFlags & 0x04) {
    if (offset + 1 >= response.length) { DEBUG_W("getSystemInfo FAIL at memSize offset=%u len=%u", (unsigned)offset, (unsigned)response.length); return pn5180::Status::ProtocolError; }
    info.totalBlocks = static_cast<uint16_t>(response.data[offset]) + 1;
    info.blockSize = (response.data[offset + 1] & 0x1F) + 1;
    offset += 2;
  }
  if (info.infoFlags & 0x08) {
    if (offset >= response.length) { DEBUG_W("getSystemInfo FAIL at ICref offset=%u len=%u", (unsigned)offset, (unsigned)response.length); return pn5180::Status::ProtocolError; }
    info.icReference = response.data[offset++];
  }

  info.valid = true;
  DEBUG_D("sysinfo flags=0x%02X blocks=%u blockSize=%u icRef=0x%02X",
          info.infoFlags,
          static_cast<unsigned>(info.totalBlocks),
          static_cast<unsigned>(info.blockSize),
          static_cast<unsigned>(info.icReference));
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::readSingleBlock(const uint8_t *uid, uint8_t blockNo,
                                       uint8_t *blockData, size_t blockSize) {
  if (!uid || !blockData || blockSize == 0 || blockSize > 4) {
    return pn5180::Status::InvalidArg;
  }

  uint8_t frame[11] = {
      static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH |
                           pn5180::ISO15693_FLAG_ADDRESS),
      pn5180::ISO15693_CMD_READ_SINGLE_BLOCK,
      0, 0, 0, 0, 0, 0, 0, 0,
      blockNo,
  };
  memcpy(frame + 2, uid, 8);

  pn5180::Iso15693Response response{};
  pn5180::Status st = transceiveIso15693(frame, sizeof(frame), response);
  if (st != pn5180::Status::Ok) {
    const uint8_t fallbackFrame[] = {
        static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH),
        pn5180::ISO15693_CMD_READ_SINGLE_BLOCK,
        blockNo,
    };
    DEBUG_W("readSingleBlock addressed failed block=%u st=%u, trying non-addressed fallback",
            static_cast<unsigned>(blockNo),
            static_cast<unsigned>(st));
    st = transceiveIso15693(fallbackFrame, sizeof(fallbackFrame), response);
    if (st != pn5180::Status::Ok) return st;
  }
  if (response.length < (1 + blockSize)) return pn5180::Status::ProtocolError;

  memcpy(blockData, response.data + 1, blockSize);
  DEBUG_D("readSingleBlock block=%u size=%u",
          static_cast<unsigned>(blockNo),
          static_cast<unsigned>(blockSize));
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::readMultipleBlocks(const uint8_t *uid, uint8_t firstBlock,
                                          uint8_t blockCount, uint8_t *data,
                                          size_t blockSize) {
  if (!uid || !data || blockCount == 0 || blockSize == 0 || blockSize > 4) {
    return pn5180::Status::InvalidArg;
  }

  uint8_t frame[12] = {
      static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH |
                           pn5180::ISO15693_FLAG_ADDRESS),
      pn5180::ISO15693_CMD_READ_MULTIPLE_BLOCKS,
      0, 0, 0, 0, 0, 0, 0, 0,
      firstBlock,
      static_cast<uint8_t>(blockCount - 1),
  };
  memcpy(frame + 2, uid, 8);

  pn5180::Iso15693Response response{};
  pn5180::Status st = transceiveIso15693(frame, sizeof(frame), response);
  if (st != pn5180::Status::Ok) {
    const uint8_t fallbackFrame[] = {
        static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH),
        pn5180::ISO15693_CMD_READ_MULTIPLE_BLOCKS,
        firstBlock,
        static_cast<uint8_t>(blockCount - 1),
    };
    DEBUG_W("readMultipleBlocks addressed failed first=%u count=%u st=%u, trying non-addressed fallback",
            static_cast<unsigned>(firstBlock),
            static_cast<unsigned>(blockCount),
            static_cast<unsigned>(st));
    st = transceiveIso15693(fallbackFrame, sizeof(fallbackFrame), response);
    if (st != pn5180::Status::Ok) return st;
  }

  const size_t expectedLen = 1 + static_cast<size_t>(blockCount) * blockSize;
  if (response.length < expectedLen) return pn5180::Status::ProtocolError;

  memcpy(data, response.data + 1, static_cast<size_t>(blockCount) * blockSize);
  DEBUG_D("readMultipleBlocks first=%u count=%u size=%u",
          static_cast<unsigned>(firstBlock),
          static_cast<unsigned>(blockCount),
          static_cast<unsigned>(blockSize));
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::writeSingleBlock(const uint8_t *uid, uint8_t blockNo,
                                        const uint8_t *blockData, size_t blockSize) {
  if (!uid || !blockData || blockSize == 0 || blockSize > 4) {
    return pn5180::Status::InvalidArg;
  }

  uint8_t frame[15] = {
      static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH |
                           pn5180::ISO15693_FLAG_ADDRESS),
      pn5180::ISO15693_CMD_WRITE_SINGLE_BLOCK,
      0, 0, 0, 0, 0, 0, 0, 0,
      blockNo,
      0, 0, 0, 0,
  };
  memcpy(frame + 2, uid, 8);
  memcpy(frame + 11, blockData, blockSize);

  pn5180::Iso15693Response response{};
  pn5180::Status st = transceiveIso15693(frame, 11 + blockSize, response, 25);
  if (st != pn5180::Status::Ok) {
    uint8_t fallbackFrame[7] = {
        static_cast<uint8_t>(pn5180::ISO15693_FLAG_DATA_RATE_HIGH),
        pn5180::ISO15693_CMD_WRITE_SINGLE_BLOCK,
        blockNo,
        0, 0, 0, 0,
    };
    memcpy(fallbackFrame + 3, blockData, blockSize);
    DEBUG_W("writeSingleBlock addressed failed block=%u st=%u, trying non-addressed fallback",
            static_cast<unsigned>(blockNo),
            static_cast<unsigned>(st));
    st = transceiveIso15693(fallbackFrame, 3 + blockSize, response, 25);
    if (st != pn5180::Status::Ok) return st;
  }
  if (response.length < 1) return pn5180::Status::ProtocolError;

  DEBUG_D("writeSingleBlock block=%u size=%u",
          static_cast<unsigned>(blockNo),
          static_cast<unsigned>(blockSize));
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::sendCommand(pn5180::Command command,
                                   const uint8_t *payload, size_t payloadLen) {
  uint8_t frame[16] = {};
  if (payloadLen + 1 > sizeof(frame)) return pn5180::Status::InvalidArg;

  frame[0] = static_cast<uint8_t>(command);
  if (payload && payloadLen > 0) {
    memcpy(frame + 1, payload, payloadLen);
  }
  return _bus.writeFrame(frame, payloadLen + 1);
}

pn5180::Status Pn5180::waitForIrq(uint32_t mask, uint32_t &irqStatus,
                                  uint32_t timeoutMs) {
  const uint32_t startedAt = millis();
  irqStatus = 0;
  while (millis() - startedAt < timeoutMs) {
    pn5180::Status st = readRegister(pn5180::IRQ_STATUS, irqStatus);
    if (st != pn5180::Status::Ok) return st;
    if (irqStatus & mask) {
      return pn5180::Status::Ok;
    }
    delay(1);
  }
  return pn5180::Status::Timeout;
}

pn5180::Status Pn5180::loadTypeA106Config() {
  DEBUG_D("loadTypeA106Config");
  return loadRfConfig(0x00, 0x80);
}

pn5180::Status Pn5180::loadTypeB106Config() {
  DEBUG_D("loadTypeB106Config");
  return loadRfConfig(0x04, 0x84);
}

pn5180::Status Pn5180::loadTypeF212Config() {
  DEBUG_D("loadTypeF212Config");
  return loadRfConfig(0x08, 0x88);
}

pn5180::Status Pn5180::setCrcTx(bool enabled) {
  return enabled ? writeRegisterOrMask(pn5180::CRC_TX_CONFIG, 0x00000001ul)
                 : writeRegisterAndMask(pn5180::CRC_TX_CONFIG, 0xFFFFFFFEul);
}

pn5180::Status Pn5180::setCrcRx(bool enabled) {
  return enabled ? writeRegisterOrMask(pn5180::CRC_RX_CONFIG, 0x00000001ul)
                 : writeRegisterAndMask(pn5180::CRC_RX_CONFIG, 0xFFFFFFFEul);
}

pn5180::Status Pn5180::setRxParity(bool enabled) {
  return enabled ? writeRegisterOrMask(pn5180::CRC_RX_CONFIG, 0x00000400ul)
                 : writeRegisterAndMask(pn5180::CRC_RX_CONFIG, 0xFFFFFBFFul);
}

pn5180::Status Pn5180::transceiveRaw(const uint8_t *command, size_t commandLen,
                                     pn5180::Iso15693Response &response,
                                     uint8_t validBits, bool logTimeout,
                                     uint32_t timeoutMs,
                                     bool allowPartialRx) {
  if (!command || commandLen == 0) return pn5180::Status::InvalidArg;

  response = {};

  pn5180::Status st = clearIrqStatus();
  if (st != pn5180::Status::Ok) return st;

  st = setIdle();
  if (st != pn5180::Status::Ok) return st;

  // Flush stale RX buffer: if RX_STATUS has leftover bytes from a prior
  // transaction, read (and discard) them so the next IDLE handler doesn't
  // mistake stale data for a fresh response.
  {
    uint32_t prevRxStatus = 0;
    st = readRegister(pn5180::RX_STATUS, prevRxStatus);
    if (st != pn5180::Status::Ok) return st;
    const size_t staleBytes = prevRxStatus & pn5180::RX_STATUS_BYTES_MASK;
    if (staleBytes > 0 && staleBytes <= 64) {
      uint8_t discard[64];
      (void)readData(discard, staleBytes);
      (void)clearIrqStatus();
    }
  }

  st = setTransceive();
  if (st != pn5180::Status::Ok) return st;

  st = sendData(command, commandLen, validBits);
  if (st != pn5180::Status::Ok) return st;

  const uint32_t startedAt = millis();
  while (millis() - startedAt < timeoutMs) {
    st = readRegister(pn5180::IRQ_STATUS, response.irqStatus);
    if (st != pn5180::Status::Ok) return st;

    if (response.irqStatus &
        (pn5180::IRQ_GENERAL_ERROR | pn5180::IRQ_RF_ACTIVE_ERROR)) {
      Serial.printf("[RFID][PN5180LIB] irq error irq=0x%08lX\n",
                    static_cast<unsigned long>(response.irqStatus));
      DEBUG_W("transceive irq error irq=0x%08lX",
              static_cast<unsigned long>(response.irqStatus));
      (void)setIdle();
      clearIrqStatus();
      return pn5180::Status::ProtocolError;
    }

    if (response.irqStatus & pn5180::IRQ_RX) {
      break;
    }

    if (response.irqStatus & pn5180::IRQ_IDLE) {
      st = readRegister(pn5180::RX_STATUS, response.rxStatus);
      if (st != pn5180::Status::Ok) return st;

      const size_t rxBytes = response.rxStatus & pn5180::RX_STATUS_BYTES_MASK;
      const uint8_t rxLastBits =
          static_cast<uint8_t>((response.rxStatus >> 13) & 0x07u);

      // SOF not detected → tag never started responding → rxBytes is stale
      const bool sofSeen = (response.irqStatus & pn5180::IRQ_RX_SOF_DET) != 0;
      if (!sofSeen && (rxBytes > 0 || rxLastBits != 0)) {
        DEBUG_W("transceive IDLE stale rxStatus=0x%08lX irq=0x%08lX (no SOF)",
                static_cast<unsigned long>(response.rxStatus),
                static_cast<unsigned long>(response.irqStatus));
        (void)setIdle();
        clearIrqStatus();
        return pn5180::Status::Timeout;
      }

      if (rxBytes > 0 || (allowPartialRx && rxLastBits != 0)) {
        if (rxLastBits != 0 && !allowPartialRx) {
          if (logTimeout) {
            Serial.printf("[RFID][PN5180LIB] idle-partial-data irq=0x%08lX rx=0x%08lX len=%u lastBits=%u\n",
                          static_cast<unsigned long>(response.irqStatus),
                          static_cast<unsigned long>(response.rxStatus),
                          static_cast<unsigned>(rxBytes),
                          static_cast<unsigned>(rxLastBits));
          }
          if (logTimeout && shouldLogPn5180Timeout()) {
            DEBUG_W("transceive idle partial data irq=0x%08lX rx=0x%08lX len=%u lastBits=%u",
                    static_cast<unsigned long>(response.irqStatus),
                    static_cast<unsigned long>(response.rxStatus),
                    static_cast<unsigned>(rxBytes),
                    static_cast<unsigned>(rxLastBits));
          }
          (void)setIdle();
          clearIrqStatus();
          return pn5180::Status::Timeout;
        }

        response.length = rxBytes + ((rxLastBits != 0) ? 1u : 0u);
        DEBUG_D("transceive idle-with-data irq=0x%08lX rx=0x%08lX len=%u",
                static_cast<unsigned long>(response.irqStatus),
                static_cast<unsigned long>(response.rxStatus),
                static_cast<unsigned>(response.length));
        break;
      }

      if (sofSeen) {
        delay(1);
        continue;
      }

      uint32_t rfStatus = 0;
      uint32_t systemStatus = 0;
      uint32_t rxStatus = response.rxStatus;
      if (logTimeout) {
        (void)readRegister(pn5180::RF_STATUS, rfStatus);
        (void)readRegister(pn5180::SYSTEM_STATUS, systemStatus);
        Serial.printf("[RFID][PN5180LIB] idle-timeout irq=0x%08lX rf=0x%08lX system=0x%08lX rx=0x%08lX\n",
                      static_cast<unsigned long>(response.irqStatus),
                      static_cast<unsigned long>(rfStatus),
                      static_cast<unsigned long>(systemStatus),
                      static_cast<unsigned long>(rxStatus));
      }
      if (logTimeout && shouldLogPn5180Timeout()) {
        DEBUG_W("transceive idle timeout irq=0x%08lX",
                static_cast<unsigned long>(response.irqStatus));
        DEBUG_W("timeout state rf=0x%08lX system=0x%08lX rx=0x%08lX",
                static_cast<unsigned long>(rfStatus),
                static_cast<unsigned long>(systemStatus),
                static_cast<unsigned long>(rxStatus));
      }
      (void)setIdle();
      clearIrqStatus();
      return pn5180::Status::Timeout;
    }

    delay(1);
  }

  if ((response.irqStatus & pn5180::IRQ_RX) == 0 && response.length == 0) {
    uint32_t rfStatus = 0;
    uint32_t systemStatus = 0;
    (void)readRegister(pn5180::RF_STATUS, rfStatus);
    (void)readRegister(pn5180::SYSTEM_STATUS, systemStatus);
    if (logTimeout && shouldLogPn5180Timeout()) {
      DEBUG_W("transceive no RX irq=0x%08lX",
              static_cast<unsigned long>(response.irqStatus));
      DEBUG_W("no-rx state rf=0x%08lX system=0x%08lX",
              static_cast<unsigned long>(rfStatus),
              static_cast<unsigned long>(systemStatus));
    }
    (void)setIdle();
    clearIrqStatus();
    return pn5180::Status::Timeout;
  }

  if (response.length == 0 && response.rxStatus == 0) {
    st = readRegister(pn5180::RX_STATUS, response.rxStatus);
    if (st != pn5180::Status::Ok) return st;
  }
  if (response.length == 0) {
    const size_t rxBytes = response.rxStatus & pn5180::RX_STATUS_BYTES_MASK;
    const uint8_t rxLastBits =
        static_cast<uint8_t>((response.rxStatus >> 13) & 0x07u);
    response.length = rxBytes + ((rxLastBits != 0) ? 1u : 0u);
  }
  if (response.length == 0 || response.length > sizeof(response.data)) {
    DEBUG_W("transceive invalid length rxStatus=0x%08lX len=%u",
            static_cast<unsigned long>(response.rxStatus),
            static_cast<unsigned>(response.length));
    (void)setIdle();
    clearIrqStatus();
    return pn5180::Status::ProtocolError;
  }

  st = readData(response.data, response.length);
  if (st != pn5180::Status::Ok) {
    (void)setIdle();
    return st;
  }

  clearIrqStatus();
  return pn5180::Status::Ok;
}

// ── MIFARE Classic ──────────────────────────────────────────────────

pn5180::Status Pn5180::mifareAuthenticate(const uint8_t *key, uint8_t keyType,
                                          uint8_t blockAddr, const uint8_t *uid4) {
  if (!key || !uid4) return pn5180::Status::InvalidArg;
  if (keyType != pn5180::MIFARE_KEY_A && keyType != pn5180::MIFARE_KEY_B)
    return pn5180::Status::InvalidArg;

  // Clean state before auth. The caller must ensure:
  // 1. loadRfConfig(0x00, 0x80) was called to reset Crypto1 engine
  // 2. Tag is in ACTIVE state (WUPA + SELECT done via pollTypeA)
  pn5180::Status st = setIdle();
  if (st != pn5180::Status::Ok) return st;
  st = clearIrqStatus();
  if (st != pn5180::Status::Ok) return st;

  // Clear MFC_CRYPTO_ON (bit 6) in case previous auth left it set
  st = writeRegisterAndMask(pn5180::SYSTEM_CONFIG, ~(1u << 6));
  if (st != pn5180::Status::Ok) return st;

  // Enable CRC for MIFARE AUTH command framing
  st = setCrcTx(true);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(true);
  if (st != pn5180::Status::Ok) return st;

  DEBUG_D("mifareAuth block=%u uid=%02X:%02X:%02X:%02X",
          static_cast<unsigned>(blockAddr),
          uid4[0], uid4[1], uid4[2], uid4[3]);

  // PN5180 MIFARE_AUTHENTICATE (0x0C):
  //   key[6] + keyType[1] + blockAddr[1] + uid[4] = 12 bytes payload
  uint8_t payload[12];
  memcpy(payload, key, 6);
  payload[6] = keyType;
  payload[7] = blockAddr;
  memcpy(payload + 8, uid4, 4);

  st = sendCommand(pn5180::Command::MifareAuthenticate,
                   payload, sizeof(payload));
  if (st != pn5180::Status::Ok) {
    DEBUG_W("mifareAuth sendCommand failed st=%u",
            static_cast<unsigned>(st));
    return st;
  }

  // Auth completes within writeFrame (BUSY HIGH→LOW handled there).
  // Read 1-byte auth status: 0=OK, 1=auth error, 2=timeout
  uint8_t authStatus = 0xFF;
  st = _bus.readFrame(&authStatus, 1);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("mifareAuth readFrame failed st=%u",
            static_cast<unsigned>(st));
    if (st == pn5180::Status::BusyTimeout) recoverBusFault("mifareAuth/readFrame");
    return st;
  }
  if (authStatus != 0) {
    uint32_t rfPost = 0, irqPost = 0;
    readRegister(pn5180::RF_STATUS, rfPost);
    readRegister(pn5180::IRQ_STATUS, irqPost);
    DEBUG_W("mifareAuth FAIL block=%u authStatus=%u rf=0x%08lX irq=0x%08lX",
            static_cast<unsigned>(blockAddr),
            static_cast<unsigned>(authStatus),
            (unsigned long)rfPost, (unsigned long)irqPost);
    return pn5180::Status::TagError;
  }

  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::readMifareBlock(uint8_t blockAddr, uint8_t *data16,
                                       uint32_t timeoutMs) {
  if (!data16) return pn5180::Status::InvalidArg;

  // After authentication, Crypto1 is active. CRC is computed by PN5180.
  pn5180::Status st = setCrcTx(true);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(true);
  if (st != pn5180::Status::Ok) return st;

  const uint8_t cmd[] = {pn5180::MIFARE_CMD_READ, blockAddr};
  pn5180::Iso15693Response raw{};
  st = transceiveRaw(cmd, sizeof(cmd), raw, 0, true, timeoutMs);
  if (st != pn5180::Status::Ok) return st;
  if (raw.length < 16) {
    DEBUG_W("readMifareBlock short response block=%u len=%u",
            static_cast<unsigned>(blockAddr),
            static_cast<unsigned>(raw.length));
    return pn5180::Status::ProtocolError;
  }
  memcpy(data16, raw.data, 16);
  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::writeMifareBlock(uint8_t blockAddr, const uint8_t *data16,
                                        uint32_t timeoutMs) {
  if (!data16) return pn5180::Status::InvalidArg;

  // MIFARE Classic WRITE is a two-phase "compatibility write":
  //   Phase 1: PCD sends 0xA0 + blockAddr (with CRC) → tag ACKs 4 bits
  //   Phase 2: PCD sends 16 bytes data (with CRC) → tag ACKs 4 bits
  //
  // Under Crypto1, both phases must stay within the same encrypted session.
  // The PN5180 handles Crypto1 encryption transparently when MFC_CRYPTO_ON
  // is set. We must NOT read back RX data between phases — doing so causes
  // PN5180 BUSY hang. Instead we check IRQ_RX flag only and immediately
  // proceed to phase 2.

  pn5180::Status st = setCrcTx(true);
  if (st != pn5180::Status::Ok) return st;
  st = setCrcRx(false);  // ACK is 4 bits, no CRC from tag
  if (st != pn5180::Status::Ok) return st;

  // --- Phase 1: WRITE command ---
  st = clearIrqStatus();
  if (st != pn5180::Status::Ok) return st;
  st = setIdle();
  if (st != pn5180::Status::Ok) return st;
  st = setTransceive();
  if (st != pn5180::Status::Ok) return st;

  const uint8_t cmd[] = {pn5180::MIFARE_CMD_WRITE, blockAddr};
  st = sendData(cmd, sizeof(cmd), 0);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("writeMifareBlock phase1 sendData failed block=%u st=%u",
            static_cast<unsigned>(blockAddr), static_cast<unsigned>(st));
    return st;
  }

  // Wait for RX (ACK) — don't read the ACK data, just confirm it arrived
  uint32_t irqStatus = 0;
  st = waitForIrq(pn5180::IRQ_RX | pn5180::IRQ_IDLE, irqStatus, timeoutMs);
  if (st != pn5180::Status::Ok || !(irqStatus & pn5180::IRQ_RX)) {
    DEBUG_W("writeMifareBlock phase1 no ACK block=%u irq=0x%08lX st=%u",
            static_cast<unsigned>(blockAddr),
            static_cast<unsigned long>(irqStatus),
            static_cast<unsigned>(st));
    (void)setIdle();
    (void)clearIrqStatus();
    return (st != pn5180::Status::Ok) ? st : pn5180::Status::Timeout;
  }

  // --- Phase 2: send 16 bytes data ---
  // Do NOT read phase 1 ACK from RX buffer — it would cause BUSY hang.
  // Clear IRQs and immediately send data. Chip stays in TRANSCEIVE + Crypto1.
  st = clearIrqStatus();
  if (st != pn5180::Status::Ok) return st;

  st = sendData(data16, 16, 0);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("writeMifareBlock phase2 sendData failed block=%u st=%u",
            static_cast<unsigned>(blockAddr), static_cast<unsigned>(st));
    (void)setIdle();
    (void)clearIrqStatus();
    return st;
  }

  // Wait for phase 2 ACK or IDLE (tag internal write takes ~5ms)
  irqStatus = 0;
  st = waitForIrq(pn5180::IRQ_RX | pn5180::IRQ_IDLE, irqStatus, timeoutMs);

  // Any result is acceptable: RX=ACK, IDLE=implicit OK, timeout=assume written
  // We don't read back ACK data for the same reason (BUSY hang risk).
  (void)setIdle();
  (void)clearIrqStatus();

  if (st != pn5180::Status::Ok && !(irqStatus & (pn5180::IRQ_RX | pn5180::IRQ_IDLE))) {
    DEBUG_W("writeMifareBlock phase2 timeout block=%u irq=0x%08lX",
            static_cast<unsigned>(blockAddr),
            static_cast<unsigned long>(irqStatus));
    return pn5180::Status::Timeout;
  }

  return pn5180::Status::Ok;
}

pn5180::Status Pn5180::transceiveIso15693(const uint8_t *command, size_t commandLen,
                                          pn5180::Iso15693Response &response,
                                          uint32_t timeoutMs) {
  pn5180::Status st = transceiveRaw(command, commandLen, response, 0, true, timeoutMs);
  if (st != pn5180::Status::Ok) return st;

  if (response.data[0] & 0x01) {
    DEBUG_W("tag returned error flag firstByte=0x%02X", response.data[0]);
    return pn5180::Status::TagError;
  }

  return pn5180::Status::Ok;
}

void Pn5180::writeLe32(uint8_t *dst, uint32_t value) {
  dst[0] = static_cast<uint8_t>(value & 0xFF);
  dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  dst[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  dst[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

uint32_t Pn5180::readLe32(const uint8_t *src) {
  return static_cast<uint32_t>(src[0]) |
         (static_cast<uint32_t>(src[1]) << 8) |
         (static_cast<uint32_t>(src[2]) << 16) |
         (static_cast<uint32_t>(src[3]) << 24);
}
