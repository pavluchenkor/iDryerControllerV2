# HX711 Weight Sensor Connection

## Connection via RJ45

A step-down converter must be used.
On the port 5V (2(O)-VCC, 8(Br) - GND), HX711 supply voltage is 3.3V

| Connector | Purpose |
|---------|-----------|
| 1(WO)  | not connected |
| 2(O)   | VCC (5V) |
| 3(WG)  | SCALE_S0 |
| 4(B)   | SCALE_S1 |
| 5(WB)  | SCALE_SCK |
| 6(G)   | SCALE_DT |
| 7(WBr) | not connected |
| 8(Br)  | GND |

## Connection to controller

- SCALE_DT   → H2
- SCALE_SCK  → FAN2
- SCALE_S0   → SRV2
- SCALE_S1   → T2

## RJ45 Scheme

```
| DT | SCK | S0 | S1 | VCC | GND |
  |     |     |    |    |     |   
 (G)  (WB)  (WG)  (B)  (O)   (Br)
```
