# iDryer Microcontroller API

This document describes how the hardware dryer controller communicates with the iDryer backend portal via REST and WebSocket. All examples are for production (`https://portal.idryer.org`) and local development (`http://localhost:3000`). Exchange format is JSON in UTF-8 encoding.

## 1. Communication architecture
- **REST (HTTP/HTTPS)** — used only for initial device registration and claim verification. Base URL: `https://portal.idryer.org/api` (in dev `http://localhost:3000`).
- **WebSocket (Socket.IO v4)** — main telemetry/command channel. Endpoint: `wss://portal.idryer.org/socket.io` (or `ws://localhost:3000/socket.io`). Uses `websocket` transport with device token authorization.
- **Identifiers**: each device stores a permanent `token` (generated at manufacturing) and receives `deviceId` (UUID from DB) after claiming.

## 2. Device registration and claiming process
1. **Request PIN on device**  
   `POST /devices/register` (public, no JWT). Body (`RegisterDeviceDto`, `backend/src/devices/dto/register-device.dto.ts`):
   ```json
   {
     "token": "unique-hardware-token",
     "serialNumber": "IDRYER-PRO-0001"
   }
   ```
   Response (`devices.service.registerUnclaimedDevice`, `backend/src/devices/devices.service.ts:229`):
   ```json
   {
     "pin": "12345678",
     "expiresAt": "2025-01-18T12:34:56.000Z",
     "remainingSeconds": 599
   }
   ```
   PIN is exactly 8 digits and valid for 10 minutes. Re-requesting before expiry extends `remainingSeconds`.

2. **User enters PIN**  
   Portal calls `POST /devices/claim` with PIN and friendly name (`ClaimDeviceDto`). Device doesn't call this step but should display PIN and track status.

3. **Periodically check claim status**  
   `GET /devices/check-claim/:token` (public). Until claimed — `404` with `{"claimed": false}` (`devices.controller.ts:99`). After claiming, response:
   ```json
   {
     "claimed": true,
     "device": {
       "id": "2a1d...c3",
       "name": "iDryer Pro in workshop",
       "serialNumber": "IDRYER-PRO-0001",
       "token": "unique-hardware-token",
       "createdAt": "2025-01-18T12:35:42.000Z"
     }
   }
   ```
   Received `device.id` and `token` must be saved to flash/EEPROM; these are used for WebSocket connection.

## 3. WebSocket API
All messages flow through Socket.IO. Client initiates connection and immediately sends `device:connect` event.
