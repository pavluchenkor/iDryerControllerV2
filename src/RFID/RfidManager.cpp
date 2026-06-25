#include "RfidManager.h"
#include <Arduino.h>

#define LOG_LEVEL LOG_LEVEL_INFO
#define LOG_TAG "RFID"
#include <debug_log.h>

RfidManager rfidManager;

uint8_t RfidManager::addReader(IRfidDriver *driver, uint8_t unitId) {
    if (_count >= RFID_MAX_READERS) return 0xFF;
    uint8_t id = _count++;
    _drivers[id] = driver;
    _unitIds[id]  = unitId;
    return id;
}

void RfidManager::begin() {
    for (uint8_t i = 0; i < _count; i++) {
        Serial.printf("[RFID] reader%u (unit%u): ", i, _unitIds[i]);
        _available[i] = _drivers[i] && _drivers[i]->init();
        Serial.println(_available[i] ? "OK" : "not found");
    }
}

bool RfidManager::isAvailable(uint8_t readerId) const {
    return readerId < _count && _available[readerId];
}

uint8_t RfidManager::unitId(uint8_t readerId) const {
    return readerId < _count ? _unitIds[readerId] : 0xFF;
}

RfidStatus RfidManager::poll(uint8_t readerId, RfidTag &tag) {
    if (!isAvailable(readerId)) return RfidStatus::NOT_FOUND;
    return _drivers[readerId]->poll(tag);
}

RfidStatus RfidManager::read(uint8_t readerId, const RfidTag &tag,
                              uint8_t startPage, uint8_t *buf, size_t len) {
    if (!isAvailable(readerId)) return RfidStatus::NOT_FOUND;
    return _drivers[readerId]->readRaw(tag, startPage, buf, len);
}

RfidStatus RfidManager::write(uint8_t readerId, const RfidTag &tag,
                               uint8_t startPage, const uint8_t *data, size_t len) {
    if (!isAvailable(readerId)) return RfidStatus::NOT_FOUND;
    return _drivers[readerId]->writeRaw(tag, startPage, data, len);
}

void RfidManager::setScanProtocols(uint8_t readerId, RfidProtocolMask mask) {
    if (!isAvailable(readerId)) return;
    _drivers[readerId]->setScanProtocols(mask);
}
