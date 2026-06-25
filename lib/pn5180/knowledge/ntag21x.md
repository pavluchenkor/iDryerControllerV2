# NTAG213 / NTAG215 / NTAG216 — конспект NXP

Источник: NXP «NTAG213/215/216 NFC Forum Type 2 Tag compliant IC» (NT2H1511...), rev 3.x.
Данные, отмеченные `[ds]`, взяты непосредственно из datasheet; `[nxp-appnote]` — NTAG I²C plus TN00042; `[empirical]` — проверено на стенде iDryer.

## Память

| Чип | Всего страниц | Пользовательских | User bytes | CC[2] |
|-----|---------------|------------------|-----------|-------|
| NTAG213 | 45 | 4–39 (36) | 144 | 0x12 |
| NTAG215 | 135 | 4–129 (126) | 504 | 0x3E |
| NTAG216 | 231 | 4–225 (222) | 888 | 0x6D |

Страница = 4 байта. Карта памяти NTAG215:

```
page  содержимое
0     UID0..UID2, BCC0            (serial, read-only)
1     UID3..UID6                  (serial, read-only)
2     BCC1, internal, lock0, lock1
3     CC (Capability Container), CC[2] = 0x3E для NTAG215
4..129 пользовательские данные (504 B)
130   dynamic lock bytes + RFUI
131   CFG0  (MIRROR, AUTH0)
132   CFG1  (ACCESS, …)
133   PWD   (write-only)
134   PACK  (read-only через PWD_AUTH)
```

## Набор команд (Type 2 Tag)

| Код  | Имя              | Параметры                | Ответ                 | CRC_RX |
|------|------------------|--------------------------|-----------------------|--------|
| 0x30 | READ             | addr (1 B)               | 16 B (4 страницы)     | on     |
| 0x3A | FAST_READ        | start, end (по 1 B)      | (end-start+1)*4 B     | on     |
| 0xA2 | WRITE            | addr + 4 B данных        | 4-bit ACK/NAK         | **off** |
| 0xA0 | COMPATIBILITY_WRITE | addr                  | ACK, далее 16 B, ACK  | off    |
| 0x60 | GET_VERSION      | —                        | 8 B                   | on     |
| 0x3C | READ_SIG         | 0x00                     | 32 B ECC signature    | on     |
| 0x39 | READ_CNT         | 0x02                     | 3 B counter           | on     |
| 0x1B | PWD_AUTH         | 4 B PWD                  | 2 B PACK или NAK      | on/off |
| 0x50 0x00 | HLTA         | —                        | нет                   | —      |

Критично для PN5180: CRC_RX **должен быть выключен** при ожидании 4-битного
ACK/NAK от WRITE/COMP_WRITE, иначе аппаратная проверка CRC помечает ответ как
ошибку. `[ds]` `[empirical]`

## ACK / NAK кодировка (4 бита)

| Значение | Смысл |
|----------|-------|
| 0xA | ACK — команда принята и выполнена |
| 0x0 | NAK — неправильный аргумент / out-of-range адрес |
| 0x1 | NAK — CRC / parity error принятого кадра |
| 0x4 | NAK — invalid authentication counter overflow |
| 0x5 | NAK — EEPROM write error |

PN5180 отдаёт 4 бита как один байт через `RX_STATUS.rxLastBits != 0` при
`rxBytes == 0`. Младший ниббл содержит ACK/NAK. `[empirical]`

## Тайминги

- `t_write` (EEPROM programming, WRITE 0xA2): тип. 4.1 мс, макс. 4.8 мс `[ds]`
- `t_timeout_WRITE` (верхняя граница ожидания ACK с учётом эфира): 10 мс `[ds]`
- ACK приходит **после** завершения внутреннего цикла записи EEPROM. Отдельная
  пауза после ACK не требуется. `[ds]`
- Рекомендуемое окно ожидания ACK в драйвере: **15 мс** (запас на межкадровые задержки).

## Особые случаи WRITE

- `WRITE 0xA2` с первым байтом данных `0x88` — мгновенный NAK, запись не
  происходит (защита от коллизии со SELECT cascade tag). Избегать этого паттерна
  в первом байте страницы. `[ds]`
- Страницы 0–1 (UID) read-only, WRITE → NAK.
- Страница 2 — только биты lock0/lock1 можно менять (OR-маска).
- Страница 3 (CC) поддерживает только OR (необратимо).
- Запись в заблокированную страницу → NAK 0x0.

## Sequence для чистой записи пользовательской области (NTAG215)

```
REQA/WUPA → ATQA
Anticollision CL1 + SELECT CL1  (каскад, CT=0x88)
Anticollision CL2 + SELECT CL2  → SAK 0x00, UID 7 B
loop addr in 4..129:
    WRITE 0xA2 addr data[0..3]
    ожидание ACK (≤ 15 мс)
HLTA 0x50 0x00
```

Между записями соседних страниц HALT/re-SELECT **не нужен** — метка остаётся в
состоянии ACTIVE до явного HLTA или потери поля. `[ds]`

## Ссылки

- [NXP NTAG213/215/216 datasheet (официальный PDF)](https://www.nxp.com/docs/en/data-sheet/NTAG213_215_216.pdf)
- [Seritag — NTAG215 chip specs](https://seritag.com/learn/tech/chips/ntag215)
- [NXP TN00042 NTAG I²C plus FAQ (EEPROM timing)](https://www.nxp.com/docs/en/engineering-bulletin/TN00042.pdf)
