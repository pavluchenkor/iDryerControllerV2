# Wi-Fi

Lesen Sie den Abschnitt iDryer-Link

<!-- # Verbindung ESP32-C3 ↔ RP2040 über UART

## Drahtzuordnung (nach Farben)

| Pin-Nr | Farbe                     | Zweck |
| -----: | ------------------------- | ----- |
|      1 | WO (weiß-orange)          | SDA   |
|      2 | O (orange)                | VCC   |
|      3 | WG (weiß-grün)            | RX    |
|      4 | B (blau)                  | —     |
|      5 | WB (weiß-blau)            | TX    |
|      6 | G (grün)                  | —     |
|      7 | WBr (weiß-braun)          | SCL   |
|      8 | Br (braun)                | GND   |


## Grundidee

UART wird immer **gekreuzt** verbunden:

```
RX <-> TX
TX <-> RX
```
Und **unbedingt gemeinsamer GND**.

## Verbindung von ESP32-C3 und RP2040

### UART-Leitungen

| ESP32-C3    | Farbe | ↔ | RP2040     | Farbe |
| ----------- | ----- | - | ---------- | ----- |
| RX (GPIO 6) | WB    | ← | TX (Pin 4) | WB    |
| TX (GPIO 7) | WG    | → | RX (Pin 5) | WG    |


### RJ45


 ... | ... | RX  | TX  | ... | ... | GND | VCC

 |     |     |     |     |     |     |     |
  
1(WO) 7(WBr)3(WG) 5(WB) 6(G)  4(B)  8(Br) 2(O) -->
