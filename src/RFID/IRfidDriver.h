#pragma once
#include <stddef.h>
#include <stdint.h>

constexpr uint8_t RFID_UID_MAX_BYTES = 8;

enum class RfidStatus : uint8_t {
    OK,
    NOT_FOUND,    // модуль не обнаружен при init
    PENDING,      // операция детекта запущена, ответ ещё не готов
    UNCHANGED,    // метка та же самая, нового события нет
    NO_TAG,       // метка не поднесена
    READ_ERROR,
    WRITE_ERROR,
    SIZE_ERROR,   // буфер или тег не вмещает запрошенный объём
};

enum class RfidTagType : uint8_t {
    UNKNOWN,
    TOPAZ_GENERIC,
    ISO14443A_GENERIC,
    ISO14443B_GENERIC,
    FELICA_GENERIC,
    ISO15693_GENERIC,
    MIFARE_CLASSIC_1K,
    NTAG213,
    NTAG215,
    NTAG216,
    ICODE_SLIX2,
};

enum class RfidProtocol : uint8_t {
    Iso14443A = 1u << 0,  // TypeA: NTAG, MIFARE, Topaz, ISO14443A generic
    Iso14443B = 1u << 1,  // TypeB
    Iso15693  = 1u << 2,  // ICODE SLIX2, ISO15693 generic
    FeliCa    = 1u << 3,  // NFC-F
};
using RfidProtocolMask = uint8_t;
constexpr RfidProtocolMask RFID_PROTOCOL_ALL =
    static_cast<uint8_t>(RfidProtocol::Iso14443A) |
    static_cast<uint8_t>(RfidProtocol::Iso14443B) |
    static_cast<uint8_t>(RfidProtocol::Iso15693)  |
    static_cast<uint8_t>(RfidProtocol::FeliCa);

struct RfidTag {
    uint8_t     uid[RFID_UID_MAX_BYTES];
    uint8_t     uidLen;   // 4 (MIFARE), 7 (NTAG) или 8 (ISO15693 / SLIX2)
    RfidTagType type;
    uint8_t     blockSize;       // размер адресуемого блока в байтах
    uint8_t     firstUserBlock;  // первый блок пользовательских данных
    uint16_t    totalBlocks;     // всего адресуемых блоков
    uint16_t    userBlocks;      // число блоков, пригодных для пользовательских данных
};

/**
 * Абстрактный интерфейс RFID-драйвера.
 *
 * Текущая реализация: Pn532Driver (PN532, SPI).
 * Будущая реализация: Pn5180Driver (PN5180, SPI, + ISO 15693 / OpenPrintTag).
 *
 * Менеджер и вся бизнес-логика работают только с IRfidDriver* —
 * смена чипа не затрагивает ничего выше этого интерфейса.
 */
class IRfidDriver {
public:
    virtual ~IRfidDriver() = default;

    // Инициализация. false = чип не найден, модуль отключён.
    virtual bool init() = 0;

    // Опрос: есть ли метка в поле. Заполняет tag если есть.
    virtual RfidStatus poll(RfidTag &tag) = 0;

    // Чтение сырых байт начиная со страницы startPage.
    virtual RfidStatus readRaw(const RfidTag &tag,
                               uint8_t  startPage,
                               uint8_t *buf,
                               size_t   len) = 0;

    // Запись сырых байт начиная со страницы startPage.
    virtual RfidStatus writeRaw(const RfidTag &tag,
                                uint8_t        startPage,
                                const uint8_t *data,
                                size_t         len) = 0;

    // Ограничить опрос только перечисленными в маске протоколами.
    // По умолчанию драйвер опрашивает RFID_PROTOCOL_ALL. Не-pure virtual,
    // чтобы существующие драйверы без фильтра оставались совместимы.
    virtual void setScanProtocols(RfidProtocolMask /*mask*/) {}
};
