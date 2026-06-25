# Подключение OLED-экрана через RJ45

Использование OLED-экрана не является обязательным: устройство может работать через Link и портал.
После первой прошивки обычно используется сценарий с Link на `PORT 2`: настройка Link, подключение к Wi-Fi, и устройство появляется на портале.

При этом в ряде сценариев локальный экран удобнее или необходим:

- нет Wi-Fi в месте установки;
- требуется автономная работа без портала;
- производственные/сервисные стенды, где важен локальный интерфейс.

## Экран

![screen](../../img/hardware/screen/screen-photo.jpeg)

## Выбор модели

![screen](../../img/hardware/screen/screen.png)

При выборе экрана обязательно проверьте питание в описании модели.
Должна быть поддержка `3.3-5V`.
Экран, рассчитанный только на `5V`, повредит микроконтроллер.

[КУПИТЬ OLED 1.3](https://www.aliexpress.com/item/1005008126409440.html) 

## Распиновка RJ45

- `1 (WO)` -> `SDA`
- `2 (O)` -> `VCC`
- `3 (WG)` -> `PSH`
- `4 (B)` -> `BAK/CON`
- `5 (WB)` -> `TRA`
- `6 (G)` -> `TRB`
- `7 (WBr)` -> `SCL`
- `8 (Br)` -> `GND`

## Схема подключения

Соедините `CON-BAK` установите выводной резистор `10 кОм, 0.25W`.
Это позволяет корректно использовать функционал обеих кнопок.

Важно:

- не допускайте коротких замыканий токопроводящих частей;
- используйте термоусадку или изоленту для изоляции соединений.

![iDryer 2 OLED](../../img/hardware/rj45/rj45-wiring.png)

```text
 _______________(R10kΩ)________________
 |                                    |
CON | SDA | SCL | PSH | TRA | TRB | BAK | GND | VCC
 |     |     |     |     |     |     |     |     |
     1(WO) 7(WBr)3(WG) 5(WB) 6(G)  4(B)  8(Br) 2(O)
```

## Фото подключения

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-07.jpeg)

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-08.jpeg)

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-06.jpeg)

![iDryer 2 OLED](../../img/hardware/through-hole-screen/through-hole-photo-02.jpeg)
