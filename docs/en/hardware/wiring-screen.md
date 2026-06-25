# OLED screen connection via RJ45

Using an OLED screen is not mandatory: the device can work through Link and the portal.
After the first firmware flash, the usual scenario is Link on `PORT 2`: Link setup, Wi-Fi connection, and the device appears in the portal.

At the same time, in some scenarios a local screen is more convenient or required:

- no Wi-Fi at the installation site;
- autonomous operation without the portal is required;
- production/service stands where a local interface is important.

## Screen

![screen](../../img/hardware/screen/screen-photo.jpeg)

## Model selection

![screen](../../img/hardware/screen/screen.png)

When choosing a screen, be sure to check the power specification in the model description.
It must support `3.3-5V`.
A screen designed only for `5V` will damage the microcontroller.

[BUY OLED 1.3](https://www.aliexpress.com/item/1005008126409440.html)

## RJ45 pinout

- `1 (WO)` -> `SDA`
- `2 (O)` -> `VCC`
- `3 (WG)` -> `PSH`
- `4 (B)` -> `BAK/CON`
- `5 (WB)` -> `TRA`
- `6 (G)` -> `TRB`
- `7 (WBr)` -> `SCL`
- `8 (Br)` -> `GND`

## Wiring diagram

Connect `CON-BAK` by installing a through-hole resistor `10 kOhm, 0.25W`.
This allows correct use of both button functions.

Important:

- avoid short circuits of conductive parts;
- use heat-shrink tubing or electrical tape for insulation.

![iDryer 2 OLED](../../img/hardware/rj45/rj45-wiring.png)

```text
 _______________(R10kΩ)________________
 |                                    |
CON | SDA | SCL | PSH | TRA | TRB | BAK | GND | VCC
 |     |     |     |     |     |     |     |     |
     1(WO) 7(WBr)3(WG) 5(WB) 6(G)  4(B)  8(Br) 2(O)
```

## Wiring photos

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-07.jpeg)

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-08.jpeg)

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-06.jpeg)

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-02.jpeg)
