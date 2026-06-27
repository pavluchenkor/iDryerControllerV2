# Anschluss des HX711-Wägesensors

## Anschluss über RJ45

Ein Spannungsabwärtswandler wird verwendet.
Auf Port 5V (2(O)-VCC, 8(Br) - GND), Stromversorgung HX711 — 3.3V

| Anschluss | Zweck |
|---------|-----------|
| 1(WO)  | nicht verwendet |
| 2(O)   | VCC (5V) |
| 3(WG)  | SCALE_S0 |
| 4(B)   | SCALE_S1 |
| 5(WB)  | SCALE_SCK |
| 6(G)   | SCALE_DT |
| 7(WBr) | nicht verwendet |
| 8(Br)  | GND |

## Anschluss zum Steuergerät

- SCALE_DT   → H2
- SCALE_SCK  → FAN2
- SCALE_S0   → SRV2
- SCALE_S1   → T2

## RJ45-Schema

```
| DT | SCK | S0 | S1 | VCC | GND |
  |     |     |    |    |     |   
 (G)  (WB)  (WG)  (B)  (O)   (Br)
```
