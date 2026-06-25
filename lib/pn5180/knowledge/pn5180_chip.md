# PN5180 — выжимка для работы с драйвером

Полный datasheet: `../pn5180.txt` (Rev. 4.3, 30 октября 2025).

## Transceive state machine (ключевое)

`SYSTEM_CONFIG.COMMAND`:
- `000` IDLE
- `001` LPCD (в проекте не используется)
- `011` TRANSCEIVE

Состояния transceive (`RF_STATUS[26:24]`, даташит §Table 103):
```
0 — IDLE
1 — WaitTransmit
2 — Transmitting
3 — WaitReceive
4 — WaitForData
5 — Receiving
6 — LoopBack
```

`SendData` допустим **только** когда state machine в `WaitTransmit`. В коде:
`setIdle()` → `setTransceive()` → `sendData()`. `clearIrqStatus()` до перехода
в transceive обязательно, иначе старые флаги IRQ путают `waitForIrq()`.

## IRQ, которые реально используются

| Бит | Имя              | Когда выставляется |
|-----|------------------|--------------------|
| 0   | RX_IRQ           | Полный кадр принят (CRC OK, либо CRC_RX=off) |
| 1   | TX_IRQ           | Передача кадра завершена |
| 2   | IDLE_IRQ         | Переход state machine в IDLE (в т.ч. таймаут transceive) |
| 5   | RX_SOF_DET_IRQ   | Обнаружен SOF ответа — но данные ещё не полны |
| 7   | GENERAL_ERROR_IRQ | Протокольная / CRC-ошибка |
| 11  | RF_ACTIVE_ERROR_IRQ | Ошибка RF-поля |

Таймауты `transceiveRaw`: если после `sendData` за `timeoutMs` не пришло ни
RX, ни IDLE-с-данными — возвращаем `Status::Timeout`.

## RX_STATUS (0x13)

| Биты  | Поле              |
|-------|-------------------|
| 0..8  | `RX_NUM_BYTES_RECEIVED` (количество **целых** байт в FIFO) |
| 13..15 | `RX_LAST_BITS` (число бит в последнем неполном байте, 0 = целые байты) |

Для 4-битного ACK/NAK от NTAG: `rxBytes == 0`, `rxLastBits == 4`, байт из
`ReadData` содержит значение в младшем ниббле.

## Подводные камни, найденные на стенде

1. **CRC_RX должен быть off при WRITE NTAG (0xA2)**. ACK/NAK идёт без CRC.
   С CRC_RX=on PN5180 отбрасывает кадр как невалидный, `transceiveRaw`
   возвращает Timeout. Восстанавливать CRC_RX=on после обмена — другие
   операции (READ 0x30, ISO15693) ждут CRC.
2. **Не ждать только TX_IRQ при записи.** TX_IRQ говорит только «кадр ушёл».
   Настоящий статус — по RX_IRQ / IDLE+rxLastBits.
3. **10 мс timeout на ACK — впритык.** В worst-case t_write 4.8 мс + эфир ≈
   7 мс, но пики до 12 мс наблюдались. Ставим 15 мс.
4. **Между соседними WRITE не нужен HLTA.** Метка остаётся ACTIVE. HLTA
   только в конце сессии — иначе следующая страница пойдёт REQA-заново и
   потеряем ~8 мс на re-select.
5. **`clearIrqStatus()` после каждого обмена.** Без этого stale RX_IRQ
   триггерит следующий `waitForIrq` ложно.
6. **`BUSY` залипает при ошибке SPI**. В драйвере реализован `recoverBusFault`
   — hard reset через RST pin без рекурсии.

## Индексы LOAD_RF_CONFIG, которые используются

| TX/RX индекс | Протокол |
|--------------|----------|
| 0x00 / 0x80  | ISO14443A 106 kbps |
| 0x03 / 0x83  | ISO14443B 106 kbps |
| 0x05 / 0x85  | FeliCa 212 kbps |
| 0x0D / 0x8D  | ISO15693 ASK100 26 kbps |

(см. даташит §Table 32)
