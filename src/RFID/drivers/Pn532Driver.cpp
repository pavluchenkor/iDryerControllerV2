/**
 * @file Pn532Driver.cpp
 * @brief Реализация неблокирующего RFID-драйвера PN532 с использованием IRQ.
 */

#include "Pn532Driver.h"
#include <Arduino.h>
#include <string.h>

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_TAG "PN532"
#include <debug_log.h>

namespace {
constexpr uint8_t  PN532_DRIVER_MAX_IRQ_SLOTS = 4;
constexpr uint8_t  PN532_DRIVER_PASSIVE_RETRIES = 0x02;
constexpr uint16_t PN532_DRIVER_POLL_TIMEOUT_MS = 150;
Pn532Driver       *g_pn532IrqOwners[PN532_DRIVER_MAX_IRQ_SLOTS] = {};
} // namespace

Pn532Driver::Pn532Driver(SPIClassRP2040 &spi, GpioCs &cs, uint8_t rstPin,
                         uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin,
                         uint8_t irqPin)
    : _spi(spi), _cs(cs), _rstPin(rstPin),
      _misoPin(misoPin), _mosiPin(mosiPin), _sckPin(sckPin), _irqPin(irqPin) {}

bool Pn532Driver::init() {
    // Аппаратный сброс
    _cs.init();
    pinMode(_rstPin, OUTPUT);
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(10);

    // earlephilhower core: кастомный контроллер использует SPI1 с явным remap пинов.
    if (!_spi.setRX(_misoPin) || !_spi.setTX(_mosiPin) || !_spi.setSCK(_sckPin)) {
        DEBUG_E("invalid SPI pin mapping rx=%u tx=%u sck=%u",
                _misoPin, _mosiPin, _sckPin);
        return false;
    }
    _spi.begin();

    // cs.pin() передаём в PN532_SPI — он управляет CS через digitalWrite
    // При замене на ShiftRegCs: создать кастомный PN532Interface вместо PN532_SPI
    _transport = new PN532_SPI(_spi, _cs.pin());
    _nfc       = new PN532(*_transport);

    _nfc->begin();
    delay(10);

    uint32_t ver = _nfc->getFirmwareVersion();
    if (!ver) {
        DEBUG_W("chip not found (not connected?)");
        return false;
    }

    DEBUG_W("found v%lu.%lu", (ver >> 16) & 0xFF, (ver >> 8) & 0xFF);
    _nfc->SAMConfig();
    if (!_nfc->setPassiveActivationRetries(PN532_DRIVER_PASSIVE_RETRIES)) {
        DEBUG_W("failed to set passive retries=%u", PN532_DRIVER_PASSIVE_RETRIES);
    } else {
        DEBUG_W("passive retries=%u", PN532_DRIVER_PASSIVE_RETRIES);
    }
    _useIrq = registerIrq();
    _detectState = DetectState::IDLE;
    _irqPending = false;
    _tagPresent = false;
    memset(&_lastTag, 0, sizeof(_lastTag));
    _detectStartedAtMs = 0;
    DEBUG_W("irq mode: %s (pin=%u)", _useIrq ? "enabled" : "fallback polling", _irqPin);
    return true;
}

RfidStatus Pn532Driver::poll(RfidTag &tag) {
    if (!_nfc) return RfidStatus::NOT_FOUND;

    if (!_useIrq) {
        uint32_t startedAt = millis();
        uint8_t uid[RFID_UID_MAX_BYTES] = {};
        uint8_t uidLen  = 0;

        bool found = _nfc->readPassiveTargetID(
            PN532_MIFARE_ISO14443A, uid, &uidLen, PN532_DRIVER_POLL_TIMEOUT_MS);

        if (!found) {
            _tagPresent = false;
            uint32_t elapsed = millis() - startedAt;
            if (millis() - _lastNoTagLogAtMs >= 1000) {
                _lastNoTagLogAtMs = millis();
                DEBUG_I("polling no-tag in %lums", (unsigned long)elapsed);
            }
            return RfidStatus::NO_TAG;
        }

        memcpy(tag.uid, uid, uidLen);
        tag.uidLen = uidLen;
        tag.type   = detectType(uidLen);
        switch (tag.type) {
            case RfidTagType::NTAG213:
                tag.blockSize = 4;
                tag.firstUserBlock = 4;
                tag.totalBlocks = 45;
                tag.userBlocks = 36;
                break;
            case RfidTagType::NTAG215:
                tag.blockSize = 4;
                tag.firstUserBlock = 4;
                tag.totalBlocks = 135;
                tag.userBlocks = 126;
                break;
            case RfidTagType::NTAG216:
                tag.blockSize = 4;
                tag.firstUserBlock = 4;
                tag.totalBlocks = 231;
                tag.userBlocks = 222;
                break;
            case RfidTagType::MIFARE_CLASSIC_1K:
                tag.blockSize = 16;
                tag.firstUserBlock = 0;
                tag.totalBlocks = 64;
                tag.userBlocks = 48;
                break;
            default:
                break;
        }
        if (_tagPresent && sameTag(tag)) return RfidStatus::UNCHANGED;
        _tagPresent = true;
        _lastTag = tag;
        DEBUG_I("polling tag detected in %lums", (unsigned long)(millis() - startedAt));
        return RfidStatus::OK;
    }

    if (_detectState == DetectState::IDLE) {
        if (!startDetection()) {
            return RfidStatus::NO_TAG;
        }
        return RfidStatus::PENDING;
    }

    if (!hasPendingResponse()) {
        if (millis() - _detectStartedAtMs >= PN532_DRIVER_POLL_TIMEOUT_MS) {
            DEBUG_W("detect timeout after %ums", (unsigned)PN532_DRIVER_POLL_TIMEOUT_MS);
            _detectState = DetectState::IDLE;
        }
        return RfidStatus::PENDING;
    }

    uint8_t uid[RFID_UID_MAX_BYTES] = {};
    uint8_t uidLen  = 0;
    _irqPending = false;
    bool found = _nfc->readDetectedPassiveTargetID(uid, &uidLen, 1);
    uint32_t elapsed = millis() - _detectStartedAtMs;
    _detectState = DetectState::IDLE;

    if (!found) {
        _tagPresent = false;
        if (millis() - _lastNoTagLogAtMs >= 1000) {
            _lastNoTagLogAtMs = millis();
            DEBUG_I("irq no-tag in %lums", (unsigned long)elapsed);
        }
        return RfidStatus::NO_TAG;
    }

    memcpy(tag.uid, uid, uidLen);
    tag.uidLen = uidLen;
    tag.type   = detectType(uidLen);
    switch (tag.type) {
        case RfidTagType::NTAG213:
            tag.blockSize = 4;
            tag.firstUserBlock = 4;
            tag.totalBlocks = 45;
            tag.userBlocks = 36;
            break;
        case RfidTagType::NTAG215:
            tag.blockSize = 4;
            tag.firstUserBlock = 4;
            tag.totalBlocks = 135;
            tag.userBlocks = 126;
            break;
        case RfidTagType::NTAG216:
            tag.blockSize = 4;
            tag.firstUserBlock = 4;
            tag.totalBlocks = 231;
            tag.userBlocks = 222;
            break;
        case RfidTagType::MIFARE_CLASSIC_1K:
            tag.blockSize = 16;
            tag.firstUserBlock = 0;
            tag.totalBlocks = 64;
            tag.userBlocks = 48;
            break;
        default:
            break;
    }
    if (_tagPresent && sameTag(tag)) return RfidStatus::UNCHANGED;
    _tagPresent = true;
    _lastTag = tag;
    DEBUG_W("tag read uidLen=%u type=%u detectMs=%lu",
            uidLen, static_cast<unsigned>(tag.type), (unsigned long)elapsed);
    return RfidStatus::OK;
}

RfidStatus Pn532Driver::readRaw(const RfidTag &tag, uint8_t startPage,
                                uint8_t *buf, size_t len) {
    size_t pages = (len + 3) / 4;
    for (size_t i = 0; i < pages; i++) {
        uint8_t tmp[4];
        bool ok = false;

        if (tag.type == RfidTagType::MIFARE_CLASSIC_1K) {
            static const uint8_t keys[][6] = {
                {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF},  // factory default
                {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5}, // common alt
                {0xD3,0xF7,0xD3,0xF7,0xD3,0xF7}, // NFC Forum
                {0x00,0x00,0x00,0x00,0x00,0x00},  // blank
            };
            uint8_t block = startPage + i;
            bool authed = false;
            for (uint8_t k = 0; k < 4 && !authed; k++) {
                for (uint8_t kt = 0; kt < 2 && !authed; kt++) { // 0=KeyA 1=KeyB
                    authed = _nfc->mifareclassic_AuthenticateBlock(
                        const_cast<uint8_t*>(tag.uid), tag.uidLen,
                        block, kt, const_cast<uint8_t*>(keys[k]));
                }
            }
            if (!authed) {
                DEBUG_W("auth failed block=%u", block);
                return RfidStatus::READ_ERROR;
            }
            ok = _nfc->mifareclassic_ReadDataBlock(block, tmp);
        } else {
            // NTAG213 / NTAG215 / NTAG216 / UNKNOWN
            ok = _nfc->mifareultralight_ReadPage(startPage + i, tmp);
        }

        if (!ok) {
            DEBUG_W("read failed page=%u", startPage + i);
            return RfidStatus::READ_ERROR;
        }

        size_t copy = len - i * 4;
        if (copy > 4) copy = 4;
        memcpy(buf + i * 4, tmp, copy);
    }
    return RfidStatus::OK;
}

RfidStatus Pn532Driver::writeRaw(const RfidTag &tag, uint8_t startPage,
                                 const uint8_t *data, size_t len) {
    if (tag.type == RfidTagType::MIFARE_CLASSIC_1K) {
        // MIFARE Classic: запись не поддерживается (зашифрованы)
        DEBUG_W("write to MIFARE Classic not supported");
        return RfidStatus::WRITE_ERROR;
    }

    size_t pages = (len + 3) / 4;
    for (size_t i = 0; i < pages; i++) {
        uint8_t tmp[4] = {};
        size_t copy = len - i * 4;
        if (copy > 4) copy = 4;
        memcpy(tmp, data + i * 4, copy);

        if (!_nfc->mifareultralight_WritePage(startPage + i, tmp)) {
            DEBUG_W("write failed page=%u", startPage + i);
            return RfidStatus::WRITE_ERROR;
        }
    }
    return RfidStatus::OK;
}

RfidTagType Pn532Driver::detectType(uint8_t uidLen) {
    if (uidLen == 4) return RfidTagType::MIFARE_CLASSIC_1K;
    if (uidLen != 7) return RfidTagType::UNKNOWN;

    // Читаем Capability Container (страница 3, байт 2 — memory size):
    // 0x12 → NTAG213, 0x3E → NTAG215, 0x6D → NTAG216
    uint8_t cc[4] = {};
    if (_nfc->mifareultralight_ReadPage(3, cc)) {
        switch (cc[2]) {
            case 0x12: return RfidTagType::NTAG213;
            case 0x3E: return RfidTagType::NTAG215;
            case 0x6D: return RfidTagType::NTAG216;
        }
    }
    return RfidTagType::UNKNOWN;
}

bool Pn532Driver::startDetection() {
    if (!_nfc->startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A)) {
        DEBUG_W("start detect failed");
        _detectState = DetectState::IDLE;
        return false;
    }

    _detectState = DetectState::WAIT_RESPONSE;
    _detectStartedAtMs = millis();
    _irqPending = false;
    DEBUG_D("detect armed");
    return true;
}

void Pn532Driver::onIrq() {
    _irqPending = true;
}

bool Pn532Driver::registerIrq() {
    if (_irqPin == 0xFF) return false;

    const int irq = digitalPinToInterrupt(_irqPin);
    if (irq < 0) {
        DEBUG_W("invalid irq pin=%u", _irqPin);
        return false;
    }

    for (uint8_t i = 0; i < PN532_DRIVER_MAX_IRQ_SLOTS; i++) {
        if (g_pn532IrqOwners[i] != nullptr) continue;

        g_pn532IrqOwners[i] = this;
        _irqSlot = i;
        pinMode(_irqPin, INPUT_PULLUP);

        switch (i) {
            case 0: attachInterrupt(irq, handleIrqSlot0, FALLING); break;
            case 1: attachInterrupt(irq, handleIrqSlot1, FALLING); break;
            case 2: attachInterrupt(irq, handleIrqSlot2, FALLING); break;
            case 3: attachInterrupt(irq, handleIrqSlot3, FALLING); break;
        }
        DEBUG_W("irq registered pin=%u slot=%u", _irqPin, i);
        return true;
    }

    DEBUG_W("no irq slots left");
    return false;
}

bool Pn532Driver::hasPendingResponse() const {
    return _irqPending || (_nfc && _nfc->isReady());
}

bool Pn532Driver::sameTag(const RfidTag &tag) const {
    if (_lastTag.uidLen != tag.uidLen) return false;
    for (uint8_t i = 0; i < tag.uidLen; i++) {
        if (_lastTag.uid[i] != tag.uid[i]) return false;
    }
    return true;
}

void Pn532Driver::handleIrqSlot0() {
    if (g_pn532IrqOwners[0]) g_pn532IrqOwners[0]->onIrq();
}

void Pn532Driver::handleIrqSlot1() {
    if (g_pn532IrqOwners[1]) g_pn532IrqOwners[1]->onIrq();
}

void Pn532Driver::handleIrqSlot2() {
    if (g_pn532IrqOwners[2]) g_pn532IrqOwners[2]->onIrq();
}

void Pn532Driver::handleIrqSlot3() {
    if (g_pn532IrqOwners[3]) g_pn532IrqOwners[3]->onIrq();
}
