
/**
 * @file PN532_SPI.h
 * @brief SPI transport layer for the local PN532 fork.
 */

#ifndef __PN532_SPI_H__
#define __PN532_SPI_H__

#include <SPI.h>
#include "PN532Interface.h"

class PN532_SPI : public PN532Interface
{
public:
    /** @brief Создаёт SPI transport для PN532. */
    PN532_SPI(SPIClass &spi, uint8_t ss);

    /** @brief Инициализирует SPI transport. */
    void begin();
    /** @brief Будит PN532 коротким SPI-циклом. */
    void wakeup();
    /** @brief Проверяет ready/status флаг PN532 без чтения ответа. */
    bool isReady();
    /** @brief Отправляет команду PN532 и дожидается ACK. */
    int8_t writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);

    /** @brief Читает ответ PN532 с ограничением по времени ожидания. */
    int16_t readResponse(uint8_t buf[], uint8_t len, uint16_t timeout);

private:
    SPIClass *_spi;
    uint8_t _ss;
    uint8_t command;

    void writeFrame(const uint8_t *header, uint8_t hlen, const uint8_t *body = 0, uint8_t blen = 0);
    int8_t readAckFrame();

    inline void write(uint8_t data)
    {
        _spi->transfer(data);
    };

    inline uint8_t read()
    {
        return _spi->transfer(0);
    };
};

#endif
