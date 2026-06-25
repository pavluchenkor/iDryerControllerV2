# UART↔Web Adapter Guide

## Purpose
Link the RP2040 binary TLV protocol (`overview.md`) with the portal JSON‑API over Socket.IO (`device-api.md`). ESP receives UART, translates frames, and publishes to WebSocket without manually processing each menu item.

## Data sources
- **Menu schema**: use `src/menu/menu_v2.yaml` (or generated headers) to build at ESP compile time a lookup table `{ menuId -> { name, type, unit } }`. Store it in flash as `menu_map.json`.
- **Telemetry keys**: mirror `telemetry_keys.h` to JSON directory with names and units.

## UART → JSON flow
1. Parse TLV frame into `{ msgType, msgId, tlv[] }`. Reject bad CRC, version, unknown TLV types.
2. Substitute values via lookup tables:
   - `MENU_ID` → `menu.name`, `menu.type`.
   - `VALUE` → cast to float/int/bool per metadata.
   - `CTRL_INDEX`, `UNIT_ID`, etc. copy as numbers.
3. Form normalized JSON:
   ```json
   {
     "type": "SET_VALUE",
     "menuId": "0003",
     "menuName": "dryTemp",
     "ctrlIndex": 0,
     "value": 55.0,
     "raw": "AA0102..."
   }
   ```
4. Publish via Socket.IO:
   - Menu events → `telemetry:data` with `data.menu[menuName]`.
   - Telemetry TLVs → expand to DTO fields per catalog.
   - ACK/ERROR → translate to `command:result` back to portal.

## JSON → UART flow
1. Receive portal command (e.g., `command:execute`, `{ "menu": "dryTemp", "value": 55 }`).
2. Lookup menu metadata to get `menuId`, type, and controller scope.
3. Build TLV frame (`SET_VALUE` or `INVOKE_ACTION`), attach CRC16, write to UART.
4. Track `msgId` to match replies (`ACK`/`ERROR`) and send confirmation to portal.

## Errors and logging
- Keep last N raw frames for diagnostics (`logs/uart.log`). Add `raw` field to each JSON.
- If menu/key unknown, return `command:result` with `status: "unsupported"`, don't write UART.
- After desync, request `SYNC_SNAPSHOT` and refresh state cache.

This adapter isolates RP2040 firmware from portal changes: cloud stays JSON-oriented, wire carries compact TLV.
