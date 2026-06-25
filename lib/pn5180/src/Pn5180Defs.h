#pragma once

#include <stddef.h>
#include <stdint.h>

namespace pn5180 {

constexpr uint32_t kSpiClockHz = 5000000;
constexpr uint32_t kBusyTimeoutMs = 100;
constexpr uint32_t kCommandTimeoutMs = 100;
constexpr uint32_t kResetLowMs = 2;
constexpr uint32_t kResetSettleMs = 10;
constexpr size_t kInventoryResponseMaxBytes = 32;

enum class Status : uint8_t {
  Ok = 0,
  BusyTimeout,
  Timeout,
  InvalidArg,
  ProtocolError,
  TagError,
};

enum class Command : uint8_t {
  WriteRegister = 0x00,
  WriteRegisterOrMask = 0x01,
  WriteRegisterAndMask = 0x02,
  ReadRegister = 0x04,
  WriteData = 0x08,
  SendData = 0x09,
  ReadData = 0x0A,
  MifareAuthenticate = 0x0C,
  LoadRfConfig = 0x11,
  RfOn = 0x16,
  RfOff = 0x17,
};

enum Register : uint8_t {
  SYSTEM_CONFIG = 0x00,
  IRQ_ENABLE = 0x01,
  IRQ_STATUS = 0x02,
  IRQ_CLEAR = 0x03,
  CRC_RX_CONFIG = 0x12,
  RX_STATUS = 0x13,
  CRC_TX_CONFIG = 0x19,
  RF_STATUS = 0x1D,
  SYSTEM_STATUS = 0x24,
};

constexpr uint32_t IRQ_RX = 1u << 0;
constexpr uint32_t IRQ_TX = 1u << 1;
constexpr uint32_t IRQ_IDLE = 1u << 2;
constexpr uint32_t IRQ_TX_RFOFF = 1u << 8;
constexpr uint32_t IRQ_TX_RFON = 1u << 9;
constexpr uint32_t IRQ_RX_SOF_DET = 1u << 14;
constexpr uint32_t IRQ_GENERAL_ERROR = 1u << 17;
constexpr uint32_t IRQ_RF_ACTIVE_ERROR = 1u << 10;
constexpr uint32_t IRQ_ALL_CLEARABLE = 0x000FFFFFul;

constexpr uint32_t SYSTEM_CONFIG_COMMAND_MASK = 0x00000007ul;
constexpr uint32_t SYSTEM_CONFIG_COMMAND_IDLE = 0x00000000ul;
constexpr uint32_t SYSTEM_CONFIG_COMMAND_TRANSCEIVE = 0x00000003ul;

constexpr uint32_t RX_STATUS_BYTES_MASK = 0x000001FFul;
constexpr uint32_t RF_STATUS_TRANSCEIVE_STATE_MASK = 0x07000000ul;
constexpr uint32_t RF_STATUS_TRANSCEIVE_STATE_SHIFT = 24;

constexpr uint8_t ISO15693_FLAG_SUBCARRIER_SINGLE = 0x00;
constexpr uint8_t ISO15693_FLAG_DATA_RATE_HIGH = 0x02;
constexpr uint8_t ISO15693_FLAG_INVENTORY = 0x04;
constexpr uint8_t ISO15693_FLAG_PROT_EXT = 0x08;
constexpr uint8_t ISO15693_FLAG_SELECT = 0x10;
constexpr uint8_t ISO15693_FLAG_ADDRESS = 0x20;
constexpr uint8_t ISO15693_FLAG_OPTION = 0x40;
constexpr uint8_t ISO15693_FLAG_1_SLOT = 0x20;

// MIFARE Classic
constexpr uint8_t MIFARE_KEY_A = 0x60;
constexpr uint8_t MIFARE_KEY_B = 0x61;
constexpr uint8_t MIFARE_CMD_READ = 0x30;
constexpr uint8_t MIFARE_CMD_WRITE = 0xA0;
constexpr uint8_t MIFARE_BLOCK_SIZE = 16;
constexpr uint8_t MIFARE_1K_SECTORS = 16;
constexpr uint8_t MIFARE_BLOCKS_PER_SECTOR = 4;

constexpr uint8_t ISO15693_CMD_INVENTORY = 0x01;
constexpr uint8_t ISO15693_CMD_STAY_QUIET = 0x02;
constexpr uint8_t ISO15693_CMD_READ_SINGLE_BLOCK = 0x20;
constexpr uint8_t ISO15693_CMD_WRITE_SINGLE_BLOCK = 0x21;
constexpr uint8_t ISO15693_CMD_READ_MULTIPLE_BLOCKS = 0x23;
constexpr uint8_t ISO15693_CMD_GET_SYSTEM_INFO = 0x2B;

struct InventoryResponse {
  bool tagFound = false;
  uint32_t irqStatus = 0;
  uint32_t rxStatus = 0;
  size_t length = 0;
  uint8_t data[kInventoryResponseMaxBytes] = {};
};

struct Iso15693Response {
  uint32_t irqStatus = 0;
  uint32_t rxStatus = 0;
  size_t length = 0;
  uint8_t data[64] = {};
};

struct SystemInfo {
  bool valid = false;
  uint8_t infoFlags = 0;
  uint8_t uid[8] = {};
  uint8_t dsfid = 0;
  uint8_t afi = 0;
  uint16_t totalBlocks = 0;
  uint8_t blockSize = 0;
  uint8_t icReference = 0;
};

struct TypeAResponse {
  bool tagFound = false;
  uint8_t atqa[2] = {};
  uint8_t uid[10] = {};
  uint8_t uidLen = 0;
  uint8_t sak = 0;
  bool isTopaz = false;
  uint8_t hr[2] = {};
};

struct TypeBResponse {
  bool tagFound = false;
  uint8_t pupi[4] = {};
  uint8_t appData[4] = {};
  uint8_t protocolInfo[3] = {};
  uint8_t afi = 0;
};

struct TypeFResponse {
  bool tagFound = false;
  uint8_t idm[8] = {};
  uint8_t pmm[8] = {};
  uint8_t systemCode[2] = {};
  uint8_t uidLen = 8;
};

} // namespace pn5180
