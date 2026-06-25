#pragma once

#include "Pn5180Bus.h"

class Pn5180 {
public:
  Pn5180(SPIClassRP2040 &spi, uint8_t nssPin, uint8_t busyPin, uint8_t rstPin,
         uint8_t misoPin, uint8_t mosiPin, uint8_t sckPin);

  bool begin();
  void hardReset();

  pn5180::Status readRegister(uint8_t address, uint32_t &value);
  pn5180::Status writeRegister(uint8_t address, uint32_t value);
  pn5180::Status writeRegisterOrMask(uint8_t address, uint32_t mask);
  pn5180::Status writeRegisterAndMask(uint8_t address, uint32_t mask);
  pn5180::Status clearIrqStatus(uint32_t mask = pn5180::IRQ_ALL_CLEARABLE);

  pn5180::Status loadRfConfig(uint8_t txConfig, uint8_t rxConfig);
  pn5180::Status fieldOn();
  pn5180::Status fieldOff();
  pn5180::Status setIdle();
  pn5180::Status setTransceive();
  pn5180::Status sendData(const uint8_t *data, size_t len, uint8_t validBits = 0);
  pn5180::Status readData(uint8_t *data, size_t len);

  pn5180::Status inventory(pn5180::InventoryResponse &response,
                           uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status pollTypeA(pn5180::TypeAResponse &response,
                           uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status pollTypeB(pn5180::TypeBResponse &response,
                           uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status pollTypeF(pn5180::TypeFResponse &response,
                           uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status readTypeAPage(uint8_t page, uint8_t *pageData,
                               uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status writeTypeAPage(uint8_t page, const uint8_t *pageData,
                                uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status getSystemInfo(const uint8_t *uid, pn5180::SystemInfo &info,
                               uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status readSingleBlock(const uint8_t *uid, uint8_t blockNo,
                                 uint8_t *blockData, size_t blockSize = 4);
  pn5180::Status readMultipleBlocks(const uint8_t *uid, uint8_t firstBlock,
                                    uint8_t blockCount, uint8_t *data,
                                    size_t blockSize = 4);
  pn5180::Status writeSingleBlock(const uint8_t *uid, uint8_t blockNo,
                                  const uint8_t *blockData, size_t blockSize = 4);

  // MIFARE Classic
  pn5180::Status mifareAuthenticate(const uint8_t *key, uint8_t keyType,
                                    uint8_t blockAddr, const uint8_t *uid4);
  pn5180::Status readMifareBlock(uint8_t blockAddr, uint8_t *data16,
                                 uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status writeMifareBlock(uint8_t blockAddr, const uint8_t *data16,
                                  uint32_t timeoutMs = pn5180::kCommandTimeoutMs);

private:
  Pn5180Bus _bus;
  bool _rfFieldEnabled = false;
  bool _recovering = false;

  pn5180::Status loadTypeA106Config();
  pn5180::Status loadTypeB106Config();
  pn5180::Status loadTypeF212Config();
  pn5180::Status setCrcTx(bool enabled);
  pn5180::Status setCrcRx(bool enabled);
  pn5180::Status setRxParity(bool enabled);
  pn5180::Status sendCommand(pn5180::Command command,
                             const uint8_t *payload = nullptr,
                             size_t payloadLen = 0);
  pn5180::Status waitForIrq(uint32_t mask, uint32_t &irqStatus,
                            uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  pn5180::Status transceiveRaw(const uint8_t *command, size_t commandLen,
                               pn5180::Iso15693Response &response,
                               uint8_t validBits = 0,
                               bool logTimeout = true,
                               uint32_t timeoutMs = pn5180::kCommandTimeoutMs,
                               bool allowPartialRx = false);
  pn5180::Status transceiveIso15693(const uint8_t *command, size_t commandLen,
                                    pn5180::Iso15693Response &response,
                                    uint32_t timeoutMs = pn5180::kCommandTimeoutMs);
  void recoverBusFault(const char *context);
  static void writeLe32(uint8_t *dst, uint32_t value);
  static uint32_t readLe32(const uint8_t *src);
};
