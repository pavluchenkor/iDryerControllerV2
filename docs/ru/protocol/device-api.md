# API микроконтроллера iDryer

Документ описывает, как «железный» контроллер сушилки общается с backend-порталом iDryer через REST и WebSocket. Все примеры даны для production (`https://portal.idryer.org`) и локальной разработки (`http://localhost:3000`). Формат обмена — JSON в кодировке UTF-8.

## 1. Архитектура обмена
- **REST (HTTP/HTTPS)** — используется только для первичной регистрации устройства и проверки привязки. Базовый URL: `https://portal.idryer.org/api` (в деве `http://localhost:3000`).
- **WebSocket (Socket.IO v4)** — основной канал телеметрии/команд. Точка подключения: `wss://portal.idryer.org/socket.io` (или `ws://localhost:3000/socket.io`). Используется transport `websocket`, авторизация токеном устройства.
- **Идентификаторы**: каждое устройство хранит постоянный `token` (генерируется на производстве) и получает `deviceId` (UUID из БД) после привязки.

## 2. Процесс регистрации и привязки устройства
1. **Запрос PIN на устройстве**  
   `POST /devices/register` (публично, без JWT). Тело (`RegisterDeviceDto`, `backend/src/devices/dto/register-device.dto.ts`):
   ```json
   {
     "token": "unique-hardware-token",
     "serialNumber": "IDRYER-PRO-0001"
   }
   ```
   Ответ (`devices.service.registerUnclaimedDevice`, `backend/src/devices/devices.service.ts:229`):
   ```json
   {
     "pin": "12345678",
     "expiresAt": "2025-01-18T12:34:56.000Z",
     "remainingSeconds": 599
   }
   ```
   PIN состоит ровно из 8 цифр и действует 10 минут. Повторный запрос до истечения продлевает `remainingSeconds`.

2. **Пользователь вводит PIN**  
   Через портал вызывается `POST /devices/claim` c PIN и читаемым именем (`ClaimDeviceDto`). Устройство этот этап не вызывает, но должно отображать PIN и отслеживать статус.

3. **Периодический опрос статуса**  
   `GET /devices/check-claim/:token` (публично). Пока устройство не привязано — `404` c `{"claimed": false}` (`devices.controller.ts:99`). После привязки ответ:
   ```json
   {
     "claimed": true,
     "device": {
       "id": "2a1d...c3",
       "name": "iDryer Pro в мастерской",
       "serialNumber": "IDRYER-PRO-0001",
       "token": "unique-hardware-token",
       "createdAt": "2025-01-18T12:35:42.000Z"
     }
   }
   ```
   Полученные `device.id` и `token` нужно сохранить во flash/EEPROM; именно они используются при WebSocket-подключении.

## 3. WebSocket API
Все сообщения идут через Socket.IO. Клиент инициирует соединение и сразу отправляет событие `device:connect`.
