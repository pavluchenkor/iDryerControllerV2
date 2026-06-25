#ifdef NFC_INTERFACE_SPI

#include "PN532_SPI.h"
#include "PN532_debug.h"
#include "Arduino.h"

#define KASYAK_FINDER 1
#define LOG_LEVEL LOG_LEVEL_WARN
#define LOG_TAG "PN532SPI"
#include <debug_log.h>

#define STATUS_READ 2
#define DATA_WRITE 1
#define DATA_READ 3
#define PN532_SPI_RESPONSE_INLISTPASSIVETARGET 0x4B

PN532_SPI::PN532_SPI(SPIClass &spi, uint8_t ss)
{
    command = 0;
    _spi = &spi;
    _ss = ss;
}

void PN532_SPI::begin()
{
    pinMode(_ss, OUTPUT);

    _spi->begin();
    _spi->beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
    _spi->endTransaction();
#if defined(ARDUINO_XIAO_RA4M1) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
#else
    // _spi->setDataMode(SPI_MODE0); // PN532 only supports mode0
    // _spi->setBitOrder(LSBFIRST);
#if defined __SAM3X8E__
    /** DUE spi library does not support SPI_CLOCK_DIV8 macro */
    _spi->setClockDivider(42); // set clock 2MHz(max: 5MHz)
#elif defined __SAMD21G18A__
    /** M0 spi library does not support SPI_CLOCK_DIV8 macro */
    _spi->setClockDivider(24); // set clock 2MHz(max: 5MHz)
#else
    _spi->setClockDivider(SPI_CLOCK_DIV8); // set clock 2MHz(max: 5MHz)
#endif
#endif
}

void PN532_SPI::wakeup()
{
    digitalWrite(_ss, LOW);
    delay(2);
    digitalWrite(_ss, HIGH);
}

int8_t PN532_SPI::writeCommand(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen)
{
    command = header[0];
    DEBUG_D("writeCommand cmd=0x%02X hlen=%u blen=%u", command, hlen, blen);
    writeFrame(header, hlen, body, blen);

    uint8_t timeout = PN532_ACK_WAIT_TIME;
    while (!isReady())
    {
        delay(1);
        timeout--;
        if (0 == timeout)
        {
            DEBUG_W("ACK timeout cmd=0x%02X", command);
            return -2;
        }
    }
    if (readAckFrame())
    {
        DEBUG_W("invalid ACK cmd=0x%02X", command);
        return PN532_INVALID_ACK;
    }
    DEBUG_D("ACK ok cmd=0x%02X", command);
    return 0;
}

int16_t PN532_SPI::readResponse(uint8_t buf[], uint8_t len, uint16_t timeout)
{
    uint16_t time = 0;
    while (!isReady())
    {
        if (timeout != 0 && time >= timeout)
        {
            DEBUG_W("response timeout cmd=0x%02X timeout=%u", command, timeout);
            return PN532_TIMEOUT;
        }
        delay(1);
        time++;
    }

    digitalWrite(_ss, LOW);
    delay(1);

    int16_t result;
    do
    {
        write(DATA_READ);

        if (0x00 != read() || // PREAMBLE
            0x00 != read() || // STARTCODE1
            0xFF != read()    // STARTCODE2
        )
        {

            result = PN532_INVALID_FRAME;
            break;
        }

        uint8_t length = read();
        if (0 != (uint8_t)(length + read()))
        { // checksum of length
            result = PN532_INVALID_FRAME;
            break;
        }

        uint8_t cmd = command + 1; // response command
        if (PN532_PN532TOHOST != read() || (cmd) != read())
        {
            result = PN532_INVALID_FRAME;
            break;
        }

        length -= 2;
        if (length > len)
        {
            for (uint8_t i = 0; i < length; i++)
            {
                DMSG_HEX(read()); // dump message
            }
            DEBUG_W("response too large cmd=0x%02X need=%u have=%u", cmd, length, len);
            read();
            read();
            result = PN532_NO_SPACE; // not enough space
            break;
        }

        uint8_t sum = PN532_PN532TOHOST + cmd;
        for (uint8_t i = 0; i < length; i++)
        {
            buf[i] = read();
            sum += buf[i];

        }

        uint8_t checksum = read();
        if (0 != (uint8_t)(sum + checksum))
        {
            DEBUG_W("checksum failed cmd=0x%02X", cmd);
            result = PN532_INVALID_FRAME;
            break;
        }
        read(); // POSTAMBLE

        result = length;
        if (cmd == PN532_SPI_RESPONSE_INLISTPASSIVETARGET && result >= 1)
        {
            DEBUG_D("readResponse ok cmd=0x%02X len=%d nbTg=%u", cmd, result, buf[0]);
        }
        else
        {
            DEBUG_D("readResponse ok cmd=0x%02X len=%d", cmd, result);
        }
    } while (0);

    digitalWrite(_ss, HIGH);

    return result;
}

bool PN532_SPI::isReady()
{
    digitalWrite(_ss, LOW);

    write(STATUS_READ);
    uint8_t status = read() & 1;
    digitalWrite(_ss, HIGH);
    return status;
}

void PN532_SPI::writeFrame(const uint8_t *header, uint8_t hlen, const uint8_t *body, uint8_t blen)
{
    digitalWrite(_ss, LOW);
    delay(2); // wake up PN532

    write(DATA_WRITE);
    write(PN532_PREAMBLE);
    write(PN532_STARTCODE1);
    write(PN532_STARTCODE2);

    uint8_t length = hlen + blen + 1; // length of data field: TFI + DATA
    write(length);
    write(~length + 1); // checksum of length

    write(PN532_HOSTTOPN532);
    uint8_t sum = PN532_HOSTTOPN532; // sum of TFI + DATA

    for (uint8_t i = 0; i < hlen; i++)
    {
        write(header[i]);
        sum += header[i];
    }
    for (uint8_t i = 0; i < blen; i++)
    {
        write(body[i]);
        sum += body[i];
    }

    uint8_t checksum = ~sum + 1; // checksum of TFI + DATA
    write(checksum);
    write(PN532_POSTAMBLE);

    digitalWrite(_ss, HIGH);
}

int8_t PN532_SPI::readAckFrame()
{
    const uint8_t PN532_ACK[] = {0, 0, 0xFF, 0, 0xFF, 0};

    uint8_t ackBuf[sizeof(PN532_ACK)];

    digitalWrite(_ss, LOW);
    delay(1);
    write(DATA_READ);

    for (uint8_t i = 0; i < sizeof(PN532_ACK); i++)
    {
        ackBuf[i] = read();
    }

    digitalWrite(_ss, HIGH);

    return memcmp(ackBuf, PN532_ACK, sizeof(PN532_ACK));
}

#endif // NFC_INTERFACE_SPI
