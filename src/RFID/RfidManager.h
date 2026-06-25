#pragma once
#include "IRfidDriver.h"
#include <stdint.h>

constexpr uint8_t RFID_MAX_READERS = 2;

/**
 * Менеджер RFID-ридеров.
 *
 * Хранит массив IRfidDriver* — не знает про конкретные чипы.
 * Привязка readerId → unitId задаётся через конфигурацию (аналогично весам).
 */
class RfidManager {
public:
    // Добавить ридер. Вызывать до begin(). Возвращает присвоенный readerId.
    uint8_t addReader(IRfidDriver *driver, uint8_t unitId);

    // Инициализация всех ридеров. Вызвать один раз в setup().
    void begin();

    uint8_t readerCount() const { return _count; }
    bool    isAvailable(uint8_t readerId) const;
    uint8_t unitId(uint8_t readerId) const;

    // Опрос: есть ли метка у ридера readerId
    RfidStatus poll(uint8_t readerId, RfidTag &tag);

    // Чтение сырых байт
    RfidStatus read(uint8_t readerId, const RfidTag &tag,
                    uint8_t startPage, uint8_t *buf, size_t len);

    // Запись сырых байт
    RfidStatus write(uint8_t readerId, const RfidTag &tag,
                     uint8_t startPage, const uint8_t *data, size_t len);

    // Ограничить опрос указанного ридера определёнными протоколами.
    void setScanProtocols(uint8_t readerId, RfidProtocolMask mask);

private:
    IRfidDriver *_drivers[RFID_MAX_READERS] = {};
    bool         _available[RFID_MAX_READERS] = {};
    uint8_t      _unitIds[RFID_MAX_READERS] = {};
    uint8_t      _count = 0;
};

extern RfidManager rfidManager;
