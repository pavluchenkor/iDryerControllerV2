# Wi-Fi

Read the iDryer-Link section


<!-- # ESP32-C3 ↔ RP2040 Connection via UART

## Wire designation (by color)

| Pin number | Color                  | Purpose |
| -----: | --------------------- | ---------- |
|      1 | WO (white-orange)      | SDA        |
|      2 | O (orange)             | VCC        |
|      3 | WG (white-green)       | RX         |
|      4 | B (blue)               | —          |
|      5 | WB (white-blue)        | TX         |
|      6 | G (green)              | —          |
|      7 | WBr (white-brown)      | SCL        |
|      8 | Br (brown)             | GND        |


## Main principle

UART is always connected **cross-wise**:

```
RX <-> TX
TX <-> RX
```
And **common GND is mandatory**.

## How to connect ESP32-C3 and RP2040

### UART lines

| ESP32-C3    | Color | ↔ | RP2040     | Color |
| ----------- | ---- | - | ---------- | ---- |
| RX (GPIO 6) | WB   | ← | TX (pin 4) | WB   |
| TX (GPIO 7) | WG   | → | RX (pin 5) | WG   |


### RJ45

![rj45](/)

 ... | ... | RX  | TX  | ... | ... | GND | VCC
  |     |     |     |     |     |     |     |
1(WO) 7(WBr)3(WG) 5(WB) 6(G)  4(B)  8(Br) 2(O) -->
