/**
 * @file Pn532Driver.h
 * @brief IRQ-aware драйвер PN532 для RFID-подсистемы проекта.
 */

#pragma once
#include "../GpioCs.h"
#include "../IRfidDriver.h"
#include <PN532.h>
#include <PN532_SPI.h>
#include <SPI.h>

/**
 * Реализация IRfidDriver под PN532 (SPI).
 *
 * Принимает GpioCs& вместо uint8_t csPin — явно документирует
 * точку замены на ShiftRegCs при переходе на сдвиговый регистр.
 * PN532_SPI получает raw-пин через cs.pin().
 */
class Pn532Driver : public IRfidDriver {
public:
    /**
     * @brief Создаёт драйвер PN532 поверх SPI.
     * @param spi   Экземпляр SPI-шины.
     * @param cs    Линия chip-select выбранного ридера.
     * @param rstPin GPIO аппаратного сброса PN532.
     * @param misoPin GPIO MISO выбранной SPI-разводки.
     * @param mosiPin GPIO MOSI выбранной SPI-разводки.
     * @param sckPin  GPIO SCK выбранной SPI-разводки.
     * @param irqPin  GPIO линии IRQ от PN532, 0xFF если IRQ не используется.
     */
    Pn532Driver(SPIClassRP2040 &spi,
                GpioCs         &cs,
                uint8_t         rstPin,
                uint8_t         misoPin,
                uint8_t         mosiPin,
                uint8_t         sckPin,
                uint8_t         irqPin = 0xFF);

    /**
     * @brief Инициализирует PN532, SPI transport и режим IRQ/polling.
     * @return true если чип найден и готов к работе.
     */
    bool       init()                                         override;
    /**
     * @brief Выполняет один шаг конечного автомата детекта метки.
     * @param tag Кэш результата для новой найденной метки.
     * @return RFID-статус текущего шага.
     */
    RfidStatus poll(RfidTag &tag)                            override;
    /**
     * @brief Читает сырые данные пользователя с уже найденной метки.
     * @param tag Ранее найденная метка.
     * @param startPage Первая страница/блок чтения.
     * @param buf Буфер приёма.
     * @param len Количество байт.
     * @return Результат операции чтения.
     */
    RfidStatus readRaw(const RfidTag &tag, uint8_t startPage,
                       uint8_t *buf, size_t len)             override;
    /**
     * @brief Записывает сырые данные пользователя на метку.
     * @param tag Ранее найденная метка.
     * @param startPage Первая страница записи.
     * @param data Буфер исходных данных.
     * @param len Количество байт.
     * @return Результат операции записи.
     */
    RfidStatus writeRaw(const RfidTag &tag, uint8_t startPage,
                        const uint8_t *data, size_t len)     override;

private:
    SPIClassRP2040 &_spi;
    GpioCs         &_cs;
    uint8_t         _rstPin;
    uint8_t         _misoPin;
    uint8_t         _mosiPin;
    uint8_t         _sckPin;
    uint8_t         _irqPin;
    PN532_SPI      *_transport = nullptr;
    PN532          *_nfc       = nullptr;
    volatile bool   _irqPending = false;
    bool            _useIrq = false;
    bool            _tagPresent = false;
    uint8_t         _irqSlot = 0xFF;
    uint32_t        _detectStartedAtMs = 0;
    uint32_t        _lastNoTagLogAtMs = 0;
    RfidTag         _lastTag = {};

    enum class DetectState : uint8_t {
        IDLE,
        WAIT_RESPONSE,
    };

    DetectState _detectState = DetectState::IDLE;

    /**
     * @brief Определяет тип метки по UID и capability container.
     */
    RfidTagType detectType(uint8_t uidLen);
    /**
     * @brief Запускает новый цикл поиска метки.
     */
    bool        startDetection();
    /**
     * @brief ISR-hook: отмечает, что PN532 сообщил о готовом ответе.
     */
    void        onIrq();
    /**
     * @brief Регистрирует GPIO interrupt для линии IRQ.
     * @return true если IRQ успешно подключён.
     */
    bool        registerIrq();
    /**
     * @brief Проверяет, что PN532 уже подготовил ответ на текущий detect-cycle.
     */
    bool        hasPendingResponse() const;
    /**
     * @brief Сравнивает текущую метку с последней принятой меткой.
     */
    bool        sameTag(const RfidTag &tag) const;

    static void handleIrqSlot0();
    static void handleIrqSlot1();
    static void handleIrqSlot2();
    static void handleIrqSlot3();
};
