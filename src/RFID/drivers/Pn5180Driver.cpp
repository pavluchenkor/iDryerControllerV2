#include "Pn5180Driver.h"

#include <Arduino.h>
#include <string.h>

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_TAG "PN5180"
#include <debug_log.h>

Pn5180Driver::Pn5180Driver(SPIClassRP2040 &spi, GpioCs &cs, uint8_t busyPin,
                           uint8_t rstPin, uint8_t misoPin, uint8_t mosiPin,
                           uint8_t sckPin)
    : _chip(spi, cs.pin(), busyPin, rstPin, misoPin, mosiPin, sckPin) {}

namespace {
constexpr uint32_t kIso15693PollTimeoutMs = 10;
constexpr uint32_t kIso15693SystemInfoTimeoutMs = 12;
constexpr uint32_t kTypeAPollTimeoutMs = 12;
constexpr uint32_t kTypeAReadTimeoutMs = 12;
constexpr uint32_t kTypeAWriteTimeoutMs = 25;
constexpr uint8_t kTypeAWriteAttempts = 3;
constexpr uint32_t kTypeAWriteRetryDelayMs = 6;
constexpr uint32_t kTypeBPollTimeoutMs = 8;
constexpr uint32_t kTypeFPollTimeoutMs = 8;
constexpr uint32_t kMifareAuthTimeoutMs = 50;
constexpr uint32_t kMifareRwTimeoutMs = 25;
const uint8_t kMifareDefaultKey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// MIFARE Classic 1K: 16 sectors × 4 blocks = 64 blocks.
// Block 0 = manufacturer, blocks 3,7,11,...,63 = sector trailers.
// User blocks: 1,2 + 4,5,6 + 8,9,10 + ... (sector 0 has blocks 1-2 usable)
bool isMifareTrailerBlock(uint8_t block) {
  return (block % pn5180::MIFARE_BLOCKS_PER_SECTOR) == 3;
}

uint8_t mifareBlockToSector(uint8_t block) {
  return block / pn5180::MIFARE_BLOCKS_PER_SECTOR;
}

bool shouldLogPn5180PollFailure() {
  static uint32_t lastLogAtMs = 0;
  const uint32_t now = millis();
  if (now - lastLogAtMs < 1000) {
    return false;
  }
  lastLogAtMs = now;
  return true;
}

RfidProtocol pollProtocolToRfid(Pn5180Driver::PollProtocol p) {
  switch (p) {
    case Pn5180Driver::PollProtocol::Iso15693: return RfidProtocol::Iso15693;
    case Pn5180Driver::PollProtocol::TypeA:    return RfidProtocol::Iso14443A;
    case Pn5180Driver::PollProtocol::TypeB:    return RfidProtocol::Iso14443B;
    case Pn5180Driver::PollProtocol::TypeF:    return RfidProtocol::FeliCa;
  }
  return RfidProtocol::Iso15693;
}

bool maskContains(RfidProtocolMask mask, Pn5180Driver::PollProtocol p) {
  return (mask & static_cast<uint8_t>(pollProtocolToRfid(p))) != 0;
}

// Round-robin с пропуском протоколов, отсутствующих в маске.
// Порядок: Iso15693 → TypeA → TypeB → TypeF → Iso15693...
Pn5180Driver::PollProtocol advanceScanProtocol(Pn5180Driver::PollProtocol cur,
                                               RfidProtocolMask mask) {
  if (mask == 0) return cur;  // маска пустая — ничего не менять
  Pn5180Driver::PollProtocol next = cur;
  for (int i = 0; i < 4; i++) {
    switch (next) {
      case Pn5180Driver::PollProtocol::Iso15693: next = Pn5180Driver::PollProtocol::TypeA; break;
      case Pn5180Driver::PollProtocol::TypeA:    next = Pn5180Driver::PollProtocol::TypeB; break;
      case Pn5180Driver::PollProtocol::TypeB:    next = Pn5180Driver::PollProtocol::TypeF; break;
      case Pn5180Driver::PollProtocol::TypeF:    next = Pn5180Driver::PollProtocol::Iso15693; break;
    }
    if (maskContains(mask, next)) return next;
  }
  return cur;
}

Pn5180Driver::PollProtocol firstInMask(RfidProtocolMask mask) {
  if (mask & static_cast<uint8_t>(RfidProtocol::Iso15693)) return Pn5180Driver::PollProtocol::Iso15693;
  if (mask & static_cast<uint8_t>(RfidProtocol::Iso14443A)) return Pn5180Driver::PollProtocol::TypeA;
  if (mask & static_cast<uint8_t>(RfidProtocol::Iso14443B)) return Pn5180Driver::PollProtocol::TypeB;
  if (mask & static_cast<uint8_t>(RfidProtocol::FeliCa)) return Pn5180Driver::PollProtocol::TypeF;
  return Pn5180Driver::PollProtocol::Iso15693;
}

bool isTypeATagType(RfidTagType type) {
  switch (type) {
    case RfidTagType::TOPAZ_GENERIC:
    case RfidTagType::MIFARE_CLASSIC_1K:
    case RfidTagType::ISO14443A_GENERIC:
    case RfidTagType::NTAG213:
    case RfidTagType::NTAG215:
    case RfidTagType::NTAG216:
      return true;
    default:
      return false;
  }
}

const char *typeASubtypeHint(const pn5180::TypeAResponse &typeA) {
  if (typeA.isTopaz) {
    return "Topaz/Type1";
  }
  if (typeA.sak == 0x08 && typeA.uidLen == 4) {
    return "MIFARE Classic-like";
  }
  if (typeA.sak == 0x00 && typeA.uidLen == 7) {
    return "NTAG/Ultralight-like";
  }
  if (typeA.sak == 0x20) {
    return "ISO14443-4A";
  }
  return "generic";
}

void fillNtagGeometry(RfidTag &tag, RfidTagType type) {
  tag.type = type;
  tag.blockSize = 4;
  tag.firstUserBlock = 4;
  switch (type) {
    case RfidTagType::NTAG213:
      tag.totalBlocks = 45;
      tag.userBlocks = 36;
      break;
    case RfidTagType::NTAG215:
      tag.totalBlocks = 135;
      tag.userBlocks = 126;
      break;
    case RfidTagType::NTAG216:
      tag.totalBlocks = 231;
      tag.userBlocks = 222;
      break;
    default:
      break;
  }
}

void fillMifareClassic1KGeometry(RfidTag &tag) {
  tag.type = RfidTagType::MIFARE_CLASSIC_1K;
  tag.blockSize = 16;
  tag.firstUserBlock = 0;
  tag.totalBlocks = 64;
  tag.userBlocks = 48;
}

} // namespace

bool Pn5180Driver::init() {
  Serial.println("[RFID][PN5180] init start");
  DEBUG_W("init start");
  if (!_chip.begin()) {
    Serial.println("[RFID][PN5180] chip begin failed");
    DEBUG_W("chip not found");
    return false;
  }
  Serial.println("[RFID][PN5180] chip begin ok");
  DEBUG_W("chip begin ok");

  pn5180::Status st = _chip.loadRfConfig(0x0D, 0x8D);
  if (st != pn5180::Status::Ok) {
    Serial.printf("[RFID][PN5180] loadRfConfig failed st=%u\n",
                  static_cast<unsigned>(st));
    DEBUG_W("loadRfConfig failed st=%u", static_cast<unsigned>(st));
    return false;
  }
  Serial.println("[RFID][PN5180] loadRfConfig ok tx=0x0D rx=0x8D");
  DEBUG_W("loadRfConfig ok tx=0x0D rx=0x8D");

  st = _chip.fieldOn();
  if (st != pn5180::Status::Ok) {
    Serial.printf("[RFID][PN5180] fieldOn failed st=%u\n",
                  static_cast<unsigned>(st));
    DEBUG_W("fieldOn failed st=%u", static_cast<unsigned>(st));
    return false;
  }
  Serial.println("[RFID][PN5180] fieldOn ok");
  DEBUG_W("fieldOn ok");

  _tagPresent = false;
  _iso15693GeometryRetryAtMs = 0;
  _preferredProtocol = PollProtocol::Iso15693;
  _scanProtocol = PollProtocol::Iso15693;
  memset(&_lastTag, 0, sizeof(_lastTag));
  Serial.println("[RFID][PN5180] ISO15693 inventory path ready");
  DEBUG_W("ISO15693 inventory path ready");
  return true;
}

RfidStatus Pn5180Driver::poll(RfidTag &tag) {
  RfidTag detectedTag{};

  // Применяем маску: если текущий протокол выключен — сдвигаем на первый включённый.
  if (_scanMask == 0) {
    return RfidStatus::NO_TAG;
  }
  if (!maskContains(_scanMask, _preferredProtocol)) {
    _preferredProtocol = firstInMask(_scanMask);
  }
  if (!maskContains(_scanMask, _scanProtocol)) {
    _scanProtocol = firstInMask(_scanMask);
  }

  const PollProtocol protocol = _tagPresent ? _preferredProtocol : _scanProtocol;

  RfidStatus st = RfidStatus::NO_TAG;
  switch (protocol) {
    case PollProtocol::Iso15693:
      st = pollIso15693(detectedTag);
      break;
    case PollProtocol::TypeA:
      st = pollTypeA(detectedTag);
      break;
    case PollProtocol::TypeB:
      st = pollTypeB(detectedTag);
      break;
    case PollProtocol::TypeF:
      st = pollTypeF(detectedTag);
      break;
  }

  if (st != RfidStatus::OK) {
    if (shouldLogPn5180PollFailure()) {
      Serial.printf("[RFID][PN5180] poll miss protocol=%u tagPresent=%u status=%u next=%u\n",
                    static_cast<unsigned>(protocol),
                    static_cast<unsigned>(_tagPresent),
                    static_cast<unsigned>(st),
                    static_cast<unsigned>(_scanProtocol));
      DEBUG_W("poll miss protocol=%u tagPresent=%u status=%u next=%u",
              static_cast<unsigned>(protocol),
              static_cast<unsigned>(_tagPresent),
              static_cast<unsigned>(st),
              static_cast<unsigned>(_scanProtocol));
    }
    if (!_tagPresent) {
      _scanProtocol = advanceScanProtocol(protocol, _scanMask);
    } else if (protocol == _preferredProtocol) {
      _scanProtocol = _preferredProtocol;
    }
    _tagPresent = false;
    return st;
  }

  if (_tagPresent && sameTag(detectedTag)) {
    tag = _lastTag;
    return RfidStatus::UNCHANGED;
  }

  switch (detectedTag.type) {
    case RfidTagType::TOPAZ_GENERIC:
      DEBUG_W("type1 tag detected uidLen=%u protocol=Topaz/Type1", detectedTag.uidLen);
      break;
    case RfidTagType::ISO14443A_GENERIC:
      DEBUG_W("typeA tag detected uidLen=%u protocol=ISO14443A", detectedTag.uidLen);
      break;
    case RfidTagType::ISO14443B_GENERIC:
      DEBUG_W("typeB tag detected uidLen=%u protocol=ISO14443B", detectedTag.uidLen);
      break;
    case RfidTagType::FELICA_GENERIC:
      DEBUG_W("typeF tag detected uidLen=%u protocol=FeliCa/NFC-F", detectedTag.uidLen);
      break;
    default:
      break;
  }

  tag = detectedTag;
  _lastTag = detectedTag;
  _tagPresent = true;
  _scanProtocol = _preferredProtocol;
  return RfidStatus::OK;
}

RfidStatus Pn5180Driver::pollIso15693(RfidTag &tag) {
  invalidateTypeASelection();
  pn5180::Status st = _chip.loadRfConfig(0x0D, 0x8D);
  if (st != pn5180::Status::Ok) {
    DEBUG_W("ISO15693 loadRfConfig failed st=%u", static_cast<unsigned>(st));
    return RfidStatus::NO_TAG;
  }

  st = _chip.fieldOn();
  if (st != pn5180::Status::Ok) {
    DEBUG_W("ISO15693 fieldOn failed st=%u", static_cast<unsigned>(st));
    return RfidStatus::NO_TAG;
  }

  pn5180::InventoryResponse response{};
  st = _chip.inventory(response, kIso15693PollTimeoutMs);
  if (st != pn5180::Status::Ok || !response.tagFound) {
    if (shouldLogPn5180PollFailure()) {
      Serial.printf("[RFID][PN5180] ISO15693 inventory miss st=%u tagFound=%u irq=0x%08lX rx=0x%08lX len=%u\n",
                    static_cast<unsigned>(st),
                    static_cast<unsigned>(response.tagFound),
                    static_cast<unsigned long>(response.irqStatus),
                    static_cast<unsigned long>(response.rxStatus),
                    static_cast<unsigned>(response.length));
      DEBUG_W("ISO15693 inventory miss st=%u tagFound=%u irq=0x%08lX rx=0x%08lX len=%u",
              static_cast<unsigned>(st),
              static_cast<unsigned>(response.tagFound),
              static_cast<unsigned long>(response.irqStatus),
              static_cast<unsigned long>(response.rxStatus),
              static_cast<unsigned>(response.length));
    }
    return RfidStatus::NO_TAG;
  }

  tag = {};
  tag.uidLen = 8;
  memcpy(tag.uid, response.data + 2, tag.uidLen);
  if (_lastTag.uidLen == tag.uidLen &&
      memcmp(_lastTag.uid, tag.uid, tag.uidLen) == 0 &&
      _lastTag.blockSize > 0) {
    tag = _lastTag;
    _preferredProtocol = PollProtocol::Iso15693;
    return RfidStatus::OK;
  }

  const uint32_t now = millis();
  if (now >= _iso15693GeometryRetryAtMs) {
    pn5180::SystemInfo info{};
    st = _chip.getSystemInfo(tag.uid, info, kIso15693SystemInfoTimeoutMs);
    _iso15693GeometryRetryAtMs = now + 500;
    Serial.printf("[RFID][PN5180] getSystemInfo st=%u valid=%u blockSize=%u totalBlocks=%u infoFlags=0x%02X\n",
                  static_cast<unsigned>(st),
                  static_cast<unsigned>(info.valid),
                  static_cast<unsigned>(info.blockSize),
                  static_cast<unsigned>(info.totalBlocks),
                  static_cast<unsigned>(info.infoFlags));
    if (st == pn5180::Status::Ok && info.valid &&
        info.blockSize > 0 && info.blockSize <= 4 && info.totalBlocks >= 2) {
      tag.type = RfidTagType::ISO15693_GENERIC;
      tag.blockSize = info.blockSize;
      tag.totalBlocks = info.totalBlocks;
      tag.firstUserBlock = 0;
      tag.userBlocks =
          (info.totalBlocks > 0) ? static_cast<uint16_t>(info.totalBlocks - 1) : 0;
      _preferredProtocol = PollProtocol::Iso15693;
      Serial.printf("[RFID][PN5180] ISO15693 geometry uid=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X blockSize=%u totalBlocks=%u\n",
                    tag.uid[0], tag.uid[1], tag.uid[2], tag.uid[3],
                    tag.uid[4], tag.uid[5], tag.uid[6], tag.uid[7],
                    static_cast<unsigned>(tag.blockSize),
                    static_cast<unsigned>(tag.totalBlocks));
      return RfidStatus::OK;
    }
  }

  tag.type = RfidTagType::ISO15693_GENERIC;
  tag.blockSize = 0;
  tag.totalBlocks = 0;
  tag.firstUserBlock = 0;
  tag.userBlocks = 0;
  _preferredProtocol = PollProtocol::Iso15693;
  Serial.printf("[RFID][PN5180] ISO15693 inventory hit uid=%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X (geometry deferred)\n",
                tag.uid[0], tag.uid[1], tag.uid[2], tag.uid[3],
                tag.uid[4], tag.uid[5], tag.uid[6], tag.uid[7]);
  return RfidStatus::OK;
}

RfidStatus Pn5180Driver::pollTypeA(RfidTag &tag) {
  // Clear MFC_CRYPTO_ON (bit 6) before polling — a leftover Crypto1 session
  // would encrypt WUPA/REQA making the tag unreachable.
  _chip.writeRegisterAndMask(pn5180::SYSTEM_CONFIG, ~(1u << 6));
  _mifareAuthSector = 0xFF;

  pn5180::TypeAResponse typeA{};
  pn5180::Status st = _chip.pollTypeA(typeA, kTypeAPollTimeoutMs);
  if (st != pn5180::Status::Ok || !typeA.tagFound) {
    invalidateTypeASelection();
    if (shouldLogPn5180PollFailure()) {
      Serial.printf("[RFID][PN5180] TypeA miss st=%u tagFound=%u atqa=%02X%02X sak=0x%02X uidLen=%u\n",
                    static_cast<unsigned>(st),
                    static_cast<unsigned>(typeA.tagFound),
                    typeA.atqa[0], typeA.atqa[1], typeA.sak, typeA.uidLen);
      DEBUG_W("TypeA miss st=%u tagFound=%u atqa=%02X%02X sak=0x%02X uidLen=%u",
              static_cast<unsigned>(st),
              static_cast<unsigned>(typeA.tagFound),
              typeA.atqa[0], typeA.atqa[1], typeA.sak, typeA.uidLen);
    }
    return RfidStatus::NO_TAG;
  }

  cacheTypeASelection(typeA.uid, typeA.uidLen);

  tag = {};
  tag.uidLen = typeA.uidLen;
  memcpy(tag.uid, typeA.uid, tag.uidLen);

  if (_lastTag.uidLen == tag.uidLen &&
      memcmp(_lastTag.uid, tag.uid, tag.uidLen) == 0 &&
      isTypeATagType(_lastTag.type)) {
    tag = _lastTag;
    _preferredProtocol = PollProtocol::TypeA;
    return RfidStatus::OK;
  }

  if (typeA.isTopaz) {
    tag.type = RfidTagType::TOPAZ_GENERIC;
  } else if (typeA.sak == 0x08 && typeA.uidLen == 4) {
    fillMifareClassic1KGeometry(tag);
  } else if (typeA.sak == 0x00 && typeA.uidLen == 7) {
    uint8_t cc[4] = {};
    pn5180::Status ccSt = _chip.readTypeAPage(3, cc, kTypeAReadTimeoutMs);
    if (ccSt == pn5180::Status::Ok) {
      switch (cc[2]) {
        case 0x12:
          fillNtagGeometry(tag, RfidTagType::NTAG213);
          break;
        case 0x3E:
          fillNtagGeometry(tag, RfidTagType::NTAG215);
          break;
        case 0x6D:
          fillNtagGeometry(tag, RfidTagType::NTAG216);
          break;
        default:
          tag.type = RfidTagType::ISO14443A_GENERIC;
          break;
      }
      DEBUG_D("typeA cc page=%02X:%02X:%02X:%02X", cc[0], cc[1], cc[2], cc[3]);
    } else {
      tag.type = RfidTagType::ISO14443A_GENERIC;
      DEBUG_D("typeA cc read failed st=%u", static_cast<unsigned>(ccSt));
    }
  } else {
    tag.type = RfidTagType::ISO14443A_GENERIC;
  }

  _preferredProtocol = PollProtocol::TypeA;
  DEBUG_D("typeA details atqa=%02X%02X sak=0x%02X hint=%s",
          typeA.atqa[0], typeA.atqa[1], typeA.sak, typeASubtypeHint(typeA));
  return RfidStatus::OK;
}

RfidStatus Pn5180Driver::pollTypeB(RfidTag &tag) {
  invalidateTypeASelection();
  pn5180::TypeBResponse typeB{};
  pn5180::Status st = _chip.pollTypeB(typeB, kTypeBPollTimeoutMs);
  if (st != pn5180::Status::Ok || !typeB.tagFound) {
    if (shouldLogPn5180PollFailure()) {
      Serial.printf("[RFID][PN5180] TypeB miss st=%u tagFound=%u afi=0x%02X\n",
                    static_cast<unsigned>(st),
                    static_cast<unsigned>(typeB.tagFound),
                    typeB.afi);
      DEBUG_W("TypeB miss st=%u tagFound=%u afi=0x%02X",
              static_cast<unsigned>(st),
              static_cast<unsigned>(typeB.tagFound),
              typeB.afi);
    }
    return RfidStatus::NO_TAG;
  }

  tag = {};
  tag.uidLen = 4;
  memcpy(tag.uid, typeB.pupi, tag.uidLen);
  tag.type = RfidTagType::ISO14443B_GENERIC;
  _preferredProtocol = PollProtocol::TypeB;
  DEBUG_D("typeB details pupi=%02X:%02X:%02X:%02X afi=0x%02X",
          typeB.pupi[0], typeB.pupi[1], typeB.pupi[2], typeB.pupi[3], typeB.afi);
  return RfidStatus::OK;
}

RfidStatus Pn5180Driver::pollTypeF(RfidTag &tag) {
  invalidateTypeASelection();
  pn5180::TypeFResponse typeF{};
  pn5180::Status st = _chip.pollTypeF(typeF, kTypeFPollTimeoutMs);
  if (st != pn5180::Status::Ok || !typeF.tagFound) {
    if (shouldLogPn5180PollFailure()) {
      Serial.printf("[RFID][PN5180] TypeF miss st=%u tagFound=%u systemCode=%02X%02X\n",
                    static_cast<unsigned>(st),
                    static_cast<unsigned>(typeF.tagFound),
                    typeF.systemCode[0], typeF.systemCode[1]);
      DEBUG_W("TypeF miss st=%u tagFound=%u systemCode=%02X%02X",
              static_cast<unsigned>(st),
              static_cast<unsigned>(typeF.tagFound),
              typeF.systemCode[0], typeF.systemCode[1]);
    }
    return RfidStatus::NO_TAG;
  }

  tag = {};
  tag.uidLen = typeF.uidLen;
  memcpy(tag.uid, typeF.idm, tag.uidLen);
  tag.type = RfidTagType::FELICA_GENERIC;
  _preferredProtocol = PollProtocol::TypeF;
  DEBUG_D("typeF details systemCode=%02X%02X",
          typeF.systemCode[0], typeF.systemCode[1]);
  return RfidStatus::OK;
}

RfidStatus Pn5180Driver::readRaw(const RfidTag &tag, uint8_t startPage,
                                 uint8_t *buf, size_t len) {
  if (!buf || len == 0 || tag.blockSize == 0 || tag.userBlocks == 0) {
    return RfidStatus::READ_ERROR;
  }
  const size_t capacity = size_t(tag.userBlocks) * size_t(tag.blockSize);
  if (len > capacity) {
    DEBUG_W("readRaw: requested %u bytes exceeds tag capacity %u bytes", (unsigned)len, (unsigned)capacity);
    return RfidStatus::READ_ERROR;
  }

  if ((tag.type == RfidTagType::NTAG213 || tag.type == RfidTagType::NTAG215 ||
       tag.type == RfidTagType::NTAG216) &&
      tag.uidLen == 7 && tag.blockSize == 4) {
    const size_t pages = (len + 3) / 4;
    for (size_t i = 0; i < pages; i++) {
      uint8_t pageData[4] = {};
      pn5180::Status st =
          _chip.readTypeAPage(static_cast<uint8_t>(startPage + i), pageData,
                              kTypeAReadTimeoutMs);
      if (st != pn5180::Status::Ok && ensureTypeATagSelected(tag)) {
        st = _chip.readTypeAPage(static_cast<uint8_t>(startPage + i), pageData,
                                 kTypeAReadTimeoutMs);
      }
      if (st != pn5180::Status::Ok) {
        DEBUG_W("readTypeAPage failed page=%u st=%u",
                static_cast<unsigned>(startPage + i),
                static_cast<unsigned>(st));
        return RfidStatus::READ_ERROR;
      }

      size_t copyLen = len - i * 4;
      if (copyLen > 4) copyLen = 4;
      memcpy(buf + i * 4, pageData, copyLen);
    }
    return RfidStatus::OK;
  }

  if (tag.type == RfidTagType::MIFARE_CLASSIC_1K &&
      tag.uidLen == 4 && tag.blockSize == pn5180::MIFARE_BLOCK_SIZE) {
    if (!ensureTypeATagSelected(tag)) {
      DEBUG_W("readRaw mifare select failed");
      _mifareAuthSector = 0xFF;
      return RfidStatus::READ_ERROR;
    }
    size_t offset = 0;
    uint8_t block = startPage;
    while (offset < len) {
      if (isMifareTrailerBlock(block)) { block++; continue; }
      uint8_t sector = mifareBlockToSector(block);
      if (sector != _mifareAuthSector) {
        // Switching sector: must fully reset Crypto1 + re-select + re-auth.
        // Auth может транзиентно фейлиться на sector switch (timing между
        // fieldOff и re-select), поэтому до 3 попыток с повторным fieldOff.
        constexpr int kMifareAuthMaxAttempts = 3;
        uint8_t authBlock = sector * pn5180::MIFARE_BLOCKS_PER_SECTOR + 3;
        pn5180::Status ast = pn5180::Status::TagError;
        for (int attempt = 0; attempt < kMifareAuthMaxAttempts; attempt++) {
          // Skip fieldOff/re-select only on первой попытке когда tag уже
          // свежий (0xFF = только что выбран через ensureTypeATagSelected).
          if (attempt > 0 || _mifareAuthSector != 0xFF) {
            _chip.setIdle();
            _chip.clearIrqStatus();
            _chip.fieldOff();
            delay(attempt == 0 ? 5 : 15);  // подлиннее задержка на retry
            invalidateTypeASelection();
            if (!ensureTypeATagSelected(tag)) {
              DEBUG_W("readRaw mifare re-select failed for sector=%u attempt=%d",
                      (unsigned)sector, attempt);
              continue;
            }
          }
          ast = _chip.mifareAuthenticate(
              kMifareDefaultKey, pn5180::MIFARE_KEY_A, authBlock, tag.uid);
          if (ast == pn5180::Status::Ok) break;
          DEBUG_W("readRaw mifare auth failed sector=%u attempt=%d st=%u",
                  (unsigned)sector, attempt, (unsigned)ast);
        }
        if (ast != pn5180::Status::Ok) {
          _mifareAuthSector = 0xFF;
          return RfidStatus::READ_ERROR;
        }
        _mifareAuthSector = sector;
      }
      uint8_t blockData[16] = {};
      pn5180::Status st = _chip.readMifareBlock(block, blockData, kMifareRwTimeoutMs);
      if (st != pn5180::Status::Ok) {
        DEBUG_W("readMifareBlock failed block=%u st=%u", (unsigned)block, (unsigned)st);
        invalidateTypeASelection();
        return RfidStatus::READ_ERROR;
      }
      size_t remain = len - offset;
      size_t copyLen = remain > 16 ? 16 : remain;
      memcpy(buf + offset, blockData, copyLen);
      offset += copyLen;
      block++;
    }
    // Don't invalidate on success — keep auth sector cached for next call
    return RfidStatus::OK;
  }

  if ((tag.type != RfidTagType::ICODE_SLIX2 &&
       tag.type != RfidTagType::ISO15693_GENERIC) ||
      tag.uidLen != 8) {
    return RfidStatus::READ_ERROR;
  }

  const size_t fullBlocks = len / tag.blockSize;
  const size_t tailBytes = len % tag.blockSize;
  size_t offset = 0;

  while (offset < fullBlocks * tag.blockSize) {
    const uint8_t chunkBlocks =
        static_cast<uint8_t>(((fullBlocks * tag.blockSize - offset) / tag.blockSize) > 3
                                 ? 3
                                 : ((fullBlocks * tag.blockSize - offset) / tag.blockSize));
    pn5180::Status st = _chip.readMultipleBlocks(
        tag.uid,
        static_cast<uint8_t>(startPage + (offset / tag.blockSize)),
        chunkBlocks,
        buf + offset,
        tag.blockSize);
    if (st != pn5180::Status::Ok) {
      DEBUG_W("readMultipleBlocks failed block=%u count=%u st=%u",
              static_cast<unsigned>(startPage + (offset / tag.blockSize)),
              static_cast<unsigned>(chunkBlocks),
              static_cast<unsigned>(st));
      return RfidStatus::READ_ERROR;
    }
    offset += static_cast<size_t>(chunkBlocks) * tag.blockSize;
  }

  if (tailBytes > 0) {
    uint8_t block[4] = {};
    pn5180::Status st = _chip.readSingleBlock(
        tag.uid,
        static_cast<uint8_t>(startPage + fullBlocks),
        block,
        tag.blockSize);
    if (st != pn5180::Status::Ok) {
      DEBUG_W("readSingleBlock failed block=%u st=%u",
              static_cast<unsigned>(startPage + fullBlocks),
              static_cast<unsigned>(st));
      return RfidStatus::READ_ERROR;
    }
    memcpy(buf + offset, block, tailBytes);
  }

  return RfidStatus::OK;
}

RfidStatus Pn5180Driver::writeRaw(const RfidTag &tag, uint8_t startPage,
                                  const uint8_t *data, size_t len) {
  if (!data || len == 0 || tag.blockSize == 0 || tag.userBlocks == 0) {
    return RfidStatus::WRITE_ERROR;
  }
  const size_t capacity = size_t(tag.userBlocks) * size_t(tag.blockSize);
  if (len > capacity) {
    DEBUG_W("writeRaw: data %u bytes exceeds tag capacity %u bytes", (unsigned)len, (unsigned)capacity);
    return RfidStatus::WRITE_ERROR;
  }

  if ((tag.type == RfidTagType::NTAG213 || tag.type == RfidTagType::NTAG215 ||
       tag.type == RfidTagType::NTAG216) &&
      tag.uidLen == 7 && tag.blockSize == 4) {
    const size_t pages = (len + 3) / 4;
    if (!ensureTypeATagSelected(tag)) {
      DEBUG_W("writeRaw initial select failed");
      return RfidStatus::WRITE_ERROR;
    }
    for (size_t i = 0; i < pages; i++) {
      uint8_t pageData[4] = {};
      size_t copyLen = len - i * 4;
      if (copyLen > 4) copyLen = 4;
      memcpy(pageData, data + i * 4, copyLen);

      pn5180::Status st = _chip.writeTypeAPage(
          static_cast<uint8_t>(startPage + i), pageData, kTypeAWriteTimeoutMs);
      for (uint8_t attempt = 1;
           st != pn5180::Status::Ok && attempt < kTypeAWriteAttempts;
           attempt++) {
        invalidateTypeASelection();
        delay(kTypeAWriteRetryDelayMs);
        if (!ensureTypeATagSelected(tag)) {
          st = pn5180::Status::Timeout;
          continue;
        }
        st = _chip.writeTypeAPage(static_cast<uint8_t>(startPage + i), pageData,
                                  kTypeAWriteTimeoutMs);
      }
      if (st != pn5180::Status::Ok) {
        DEBUG_W("writeTypeAPage failed page=%u st=%u",
                static_cast<unsigned>(startPage + i),
                static_cast<unsigned>(st));
        return RfidStatus::WRITE_ERROR;
      }
    }
    return RfidStatus::OK;
  }

  if (tag.type == RfidTagType::MIFARE_CLASSIC_1K &&
      tag.uidLen == 4 && tag.blockSize == pn5180::MIFARE_BLOCK_SIZE) {
    if (!ensureTypeATagSelected(tag)) {
      DEBUG_W("writeRaw mifare select failed");
      _mifareAuthSector = 0xFF;
      return RfidStatus::WRITE_ERROR;
    }
    size_t offset = 0;
    uint8_t block = startPage;
    while (offset < len) {
      if (isMifareTrailerBlock(block)) { block++; continue; }
      if (block == 0) { block++; continue; } // skip manufacturer block
      uint8_t sector = mifareBlockToSector(block);
      if (sector != _mifareAuthSector) {
        // Switching sector: must fully reset Crypto1 + re-select + re-auth.
        // Retry до 3 раз — auth может транзиентно фейлиться на sector switch.
        constexpr int kMifareAuthMaxAttempts = 3;
        uint8_t authBlock = sector * pn5180::MIFARE_BLOCKS_PER_SECTOR + 3;
        pn5180::Status ast = pn5180::Status::TagError;
        for (int attempt = 0; attempt < kMifareAuthMaxAttempts; attempt++) {
          if (attempt > 0 || _mifareAuthSector != 0xFF) {
            _chip.setIdle();
            _chip.clearIrqStatus();
            _chip.fieldOff();
            delay(attempt == 0 ? 5 : 15);
            invalidateTypeASelection();
            if (!ensureTypeATagSelected(tag)) {
              DEBUG_W("writeRaw mifare re-select failed for sector=%u attempt=%d",
                      (unsigned)sector, attempt);
              continue;
            }
          }
          ast = _chip.mifareAuthenticate(
              kMifareDefaultKey, pn5180::MIFARE_KEY_A, authBlock, tag.uid);
          if (ast == pn5180::Status::Ok) break;
          DEBUG_W("writeRaw mifare auth failed sector=%u attempt=%d st=%u",
                  (unsigned)sector, attempt, (unsigned)ast);
        }
        if (ast != pn5180::Status::Ok) {
          _mifareAuthSector = 0xFF;
          return RfidStatus::WRITE_ERROR;
        }
        _mifareAuthSector = sector;
      }
      uint8_t blockData[16] = {};
      size_t remain = len - offset;
      size_t copyLen = remain > 16 ? 16 : remain;
      if (copyLen < 16) {
        // partial block: read-modify-write
        pn5180::Status rst = _chip.readMifareBlock(block, blockData, kMifareRwTimeoutMs);
        if (rst != pn5180::Status::Ok) {
          DEBUG_W("writeRaw mifare prefetch block=%u st=%u", (unsigned)block, (unsigned)rst);
          _mifareAuthSector = 0xFF;
          return RfidStatus::WRITE_ERROR;
        }
      }
      memcpy(blockData, data + offset, copyLen);
      pn5180::Status st = _chip.writeMifareBlock(block, blockData, kMifareRwTimeoutMs);
      if (st != pn5180::Status::Ok) {
        DEBUG_W("writeMifareBlock failed block=%u st=%u", (unsigned)block, (unsigned)st);
        invalidateTypeASelection();
        return RfidStatus::WRITE_ERROR;
      }
      offset += copyLen;
      block++;
    }
    // Don't invalidate on success — keep _mifareAuthSector cached so the next
    // writeRaw call for the same sector skips re-select + re-auth.
    return RfidStatus::OK;
  }

  if ((tag.type != RfidTagType::ICODE_SLIX2 &&
       tag.type != RfidTagType::ISO15693_GENERIC) ||
      tag.uidLen != 8) {
    return RfidStatus::WRITE_ERROR;
  }

  for (size_t offset = 0; offset < len; offset += tag.blockSize) {
    uint8_t block[4] = {};
    const size_t copyLen =
        ((len - offset) >= tag.blockSize) ? tag.blockSize : (len - offset);

    if (copyLen != tag.blockSize) {
      pn5180::Status readSt =
          _chip.readSingleBlock(tag.uid,
                                static_cast<uint8_t>(startPage + (offset / tag.blockSize)),
                                block,
                                tag.blockSize);
      if (readSt != pn5180::Status::Ok) {
        DEBUG_W("prefetch block failed block=%u st=%u",
                static_cast<unsigned>(startPage + (offset / tag.blockSize)),
                static_cast<unsigned>(readSt));
        return RfidStatus::WRITE_ERROR;
      }
    }

    memcpy(block, data + offset, copyLen);
    pn5180::Status st =
        _chip.writeSingleBlock(tag.uid,
                               static_cast<uint8_t>(startPage + (offset / tag.blockSize)),
                               block,
                               tag.blockSize);
    if (st != pn5180::Status::Ok) {
      DEBUG_W("writeSingleBlock failed block=%u st=%u",
              static_cast<unsigned>(startPage + (offset / tag.blockSize)),
              static_cast<unsigned>(st));
      return RfidStatus::WRITE_ERROR;
    }
    delay(10); // t_write ICODE SLIX: typ 4ms, max 8ms — wait for tag to finish
  }

  return RfidStatus::OK;
}

bool Pn5180Driver::ensureTypeATagSelected(const RfidTag &tag) {
  if (!isTypeATagType(tag.type) || tag.uidLen == 0) return false;

  constexpr uint32_t kTypeASelectionTtlMs = 1500;
  if (_typeASelectionValid &&
      _typeASelectionUidLen == tag.uidLen &&
      memcmp(_typeASelectionUid, tag.uid, tag.uidLen) == 0 &&
      (millis() - _typeASelectionAtMs) < kTypeASelectionTtlMs) {
    return true;
  }

  // Re-selecting the tag breaks any active Crypto1 session
  _mifareAuthSector = 0xFF;

  // Clear MFC_CRYPTO_ON (bit 6) before WUPA — otherwise Crypto1 encrypts
  // the WUPA frame and the tag won't respond.
  _chip.writeRegisterAndMask(pn5180::SYSTEM_CONFIG, ~(1u << 6));

  pn5180::TypeAResponse typeA{};
  const pn5180::Status st = _chip.pollTypeA(typeA, kTypeAPollTimeoutMs);
  if (st != pn5180::Status::Ok || !typeA.tagFound || typeA.uidLen != tag.uidLen) {
    invalidateTypeASelection();
    return false;
  }
  if (memcmp(typeA.uid, tag.uid, tag.uidLen) != 0) {
    invalidateTypeASelection();
    return false;
  }
  cacheTypeASelection(typeA.uid, typeA.uidLen);
  return true;
}

void Pn5180Driver::cacheTypeASelection(const uint8_t *uid, uint8_t uidLen) {
  if (!uid || uidLen == 0 || uidLen > sizeof(_typeASelectionUid)) {
    invalidateTypeASelection();
    return;
  }
  memcpy(_typeASelectionUid, uid, uidLen);
  _typeASelectionUidLen = uidLen;
  _typeASelectionAtMs = millis();
  _typeASelectionValid = true;
  // Every SELECT breaks Crypto1 session — invalidate MIFARE auth state
  _mifareAuthSector = 0xFF;
}

void Pn5180Driver::invalidateTypeASelection() {
  _typeASelectionValid = false;
  _typeASelectionUidLen = 0;
  _mifareAuthSector = 0xFF;
}

bool Pn5180Driver::sameTag(const RfidTag &tag) const {
  if (_lastTag.uidLen != tag.uidLen) return false;
  for (uint8_t i = 0; i < tag.uidLen; i++) {
    if (_lastTag.uid[i] != tag.uid[i]) return false;
  }
  return true;
}

void Pn5180Driver::setScanProtocols(RfidProtocolMask mask) {
  _scanMask = mask;
  if (mask == 0) {
    DEBUG_W("setScanProtocols mask=0 (all disabled)");
    return;
  }
  if (!maskContains(_scanMask, _preferredProtocol)) {
    _preferredProtocol = firstInMask(_scanMask);
  }
  if (!maskContains(_scanMask, _scanProtocol)) {
    _scanProtocol = firstInMask(_scanMask);
  }
  DEBUG_W("setScanProtocols mask=0x%02X preferred=%u",
          static_cast<unsigned>(mask),
          static_cast<unsigned>(_preferredProtocol));
}
