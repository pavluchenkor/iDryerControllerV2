# UART/TLV protocol for iDryerRP2040

## Purpose
Provide a unified bidirectional channel between RP2040 and bridges (ESP32, touch screen, PC) for safely reading/writing menu (`src/menu/menu_v2.yaml`) and exchanging auxiliary data (telemetry, events). The protocol is designed to work over UART and can be transparently relayed over TCP/WebSocket/HTTP.

## Frame format
| Field         | Size | Description                                                |
|----------------|--------|----------------------------------------------------------|
| Preamble       | 1      | `0xAA` — frame start                                     |
| Version        | 1      | Protocol version number (e.g., `0x01`)                  |
| Msg Type       | 1      | Message code (`HELLO`, `SET_VALUE`, `INVOKE_ACTION`…)   |
| Msg ID         | 2      | Request/response counter (little-endian)                |
| Payload Length | 1      | TLV block size in bytes                                  |
| Payload (TLV*) | N      | TLV sequence (see below)                                |
| CRC16          | 2      | CRC16-IBM from Preamble to end of Payload              |

## TLV types (base set)
| Type | Value             |
|------|----------------------|
| 0x10 | `MENU_ID` (2 bytes)  |
| 0x11 | `CTRL_INDEX` (1 byte)|
| 0x12 | `UNIT_ID` (subscription channel) |
| 0x20 | `STATUS` (1 byte)    |
| 0x30 | `VALUE` (1/2/4 bytes per `vtype`) |
| 0x40 | `TIMESTAMP` (`uint32_t`) |
| 0x50 | `TELEMETRY_KEY` (1 byte) |
| 0x51 | `TELEMETRY_VALUE` (float/uint32) |

TLV means `type | length | value`. For example, `10 02 00 03` means: type `0x10`, length `0x02`, value `0x0003` (`MENU_DRY_TEMP`).

## Message types
| MsgType | Purpose |
|---------|------------|
| 0x01 `HELLO` | Version and capability bitmask exchange (e.g., telemetry, subscriptions support). |
| 0x02 `SET_VALUE` | Write menu node values. |
| 0x03 `INVOKE_ACTION` | Invoke action node (`start_drying`, `start_storage`). |
| 0x04 `GET_VALUE` | Read current value (response is `VALUE_REPORT`). |
| 0x05 `SYNC_SNAPSHOT` | Bulk state dump (uses `menu_state_dump.*`). |
| 0x06 `SUBSCRIBE` | Subscribe to changes/telemetry. |
| 0x07 `UNSUBSCRIBE` | Unsubscribe. |
| 0x10 `MENU_EVENT` | Async notifications of value changes or action completion. |
| 0x11 `TELEMETRY` | Free channel for parameters outside menu tree. |
| 0x81 `ACK` | Acknowledgment (`STATUS=0x00` in Payload). |
| 0x82 `ERROR` | Error, TLV `STATUS` = error code, optionally TLV `VALUE` with details. |

## Example frames
### Set drying temperature = 55 °C for controller 0
```
AA 01 02 10 42 0D  10 02 00 03  11 01 00  30 04 42 5C 00 00  2B 7F
```
1. `MENU_ID = 0x0003` (`MENU_DRY_TEMP` from `src/menu/menu_ids.h:3`).  
2. `CTRL_INDEX = 0` (first controller; TLV `11 01 00`).  
3. `VALUE = 42 5C 00 00` (float 55.0).  
4. Firmware applies the value (see algorithm below), sends `ACK` with same `msg_id`.

### Start storage for controller 1
```
AA 01 03 10 43 08  10 02 00 0A  11 01 01  7A B1
```
- `MENU_ID = 0x000A` (`MENU_STORAGE_START` in `src/menu/menu_ids.h:11`).  
- `CTRL_INDEX = 1`.  
- Action with no TLV `VALUE`, firmware calls `start_storage()` and responds `ACK` or `ERROR`.

## Detailed RP2040 processing
1. **Frame parsing.** Check preamble, version, length, CRC, and parse TLV. After parsing a `SET_VALUE`, we have `MENU_ID=0x0003`, `CTRL_INDEX=0`, `VALUE=55.0f`.  
2. **Node description lookup.** `const MenuItem& item = g_menu[menu_id];` (table generated in `src/menu/menu_data.cpp:51`). For `0x0003`, this is `MENU_DRY_TEMP` (`src/menu/menu_data.cpp:70`), specifying type `MN_VALUE`, bounds, and pointer to `menu.dry_temp`.  
3. **Message type check.** `SET_VALUE` allowed only for `MN_VALUE`/`MN_TOGGLE`. If node is `SCOPE_PER_CONTROLLER`, use index from TLV `CTRL_INDEX` as `idx`.  
4. **Apply.** Use utilities from `menu_bindings`: `store_value()` and `menu_apply_by_bind()` (`src/menu/menu_bindings.cpp:114` and `151`). They:
   - convert float to target `vtype` (`VT_F32`, `VT_U16`, `VT_BOOL`, etc),  
   - write value either directly to `menu` structure field (`src/menu/menu_state.cpp:11`) or to array `[idx]`,  
   - on `persist=true`, compute EEPROM offset (`calc_eeprom_offset()`, `src/menu/menu_bindings.cpp:136`) and call `ee_store_field`.  
5. **Callbacks.** If node has `on_change`, it's auto-called (`menu_apply_by_bind()`, lines `158‑169`). For action nodes, instead of writing, call `item.action.fn()` (e.g., `start_storage()` declared in `src/menu/menu_data.cpp:17`).  
6. **Response.** On success → `ACK` (`STATUS=0x00`). Bounds/type violation or missing node → `ERROR` with error code. Async changes published as separate `MENU_EVENT` frames.

## Exchange direction
- **Incoming to RP2040 (host → MCU):** `HELLO`, `SET_VALUE`, `GET_VALUE`, `INVOKE_ACTION`, `SUBSCRIBE`, `UNSUBSCRIBE`. These change `MenuState` or trigger actions.  
- **Outgoing from RP2040 (MCU → host):** `ACK/ERROR` (one per request), `MENU_EVENT` (any value change or action end), `SYNC_SNAPSHOT` (on request or per timer), `TELEMETRY` (aux data), `KEEPALIVE` (if needed).  

ESP32/touch screen acts as a bridge: UART ↔ Wi-Fi, no own logic. Should wait for `ACK` before proxying next command from external client to avoid races, and optionally cache `MENU_EVENT` for new connections.

## Telemetry and out-of-menu data
Some parameters (sensor temps, currents, PID statuses) are outside menu tree. Can be transmitted two ways:
1. **Separate TLVs in existing messages.** For example, add `TELEMETRY_KEY`/`TELEMETRY_VALUE` to `MENU_EVENT` if tied to a specific node.
2. **Dedicated MsgType `TELEMETRY` (0x11).** Payload contains:
   - `TELEMETRY_KEY` — short ID (can define enum `TelemetryKey` in `include/telemetry_ids.h`).  
   - `UNIT_ID` or `CTRL_INDEX`, if tied to specific controller.  
   - `TELEMETRY_VALUE` (float/uint32/string — can add type to TLV).  

For subscriptions, client sends `SUBSCRIBE` with TLV `TELEMETRY_KEY` and optional `CTRL_INDEX`. MCU maintains subscriber list and sends `TELEMETRY` frames only on value changes. This way, real heater temps, fan state, errors, and process phase can be streamed.

## How menu becomes code
1. **Source YAML.** All UI described in `src/menu/menu_v2.yaml`. Running generator (`python src/menu/generator/gen_menu_v2.py src/menu/menu_v2.yaml --out src/menu`) recreates files.  
2. **Node IDs.** `src/menu/menu_ids.h:3` has `typedef enum MenuId` with all `MENU_*`. Compiler assigns sequential values from 0. These numbers go in protocol (`MENU_ID`).  
3. **State.** `src/menu/menu_state.h`/`.cpp` (`menu_state.cpp:11`) declare `MenuState menu;` structure. Has fields `dry_temp`, `storage_temp`, `preset_*`, etc., plus `initDefaults()`/`loadFromEEPROM()`/`saveToEEPROM()` (see `menu_eeprom_io.*`).  
4. **Types and bounds.** `src/menu/menu_types.h` defines `VT_F32`, `VT_U16`, `VT_BOOL`, and enum `MenuNodeType`. Appear in `g_menu` and `g_bindings` so code understands how to interpret TLV `VALUE`.  
5. **Menu tree.** `src/menu/menu_data.cpp:51` has array `g_menu[MENU__COUNT]`. Each entry describes: parent, children, node type (`MN_VALUE`, `MN_ACTION`), UI text, ranges `min/max/step`, pointer to `menu` field, pointer to `on_invoke` function. Action nodes (e.g., `MENU_DRY_START`) reference real functions (`start_drying()` declared at file top).  
6. **Bindings.** `src/menu/menu_bindings.cpp:27` generates array `g_bindings`. Entry links string `bind` (from YAML `bind:`), `vtype`, `scope`, memory pointer in `menu`, EEPROM offset, and `on_change` function. This table used by normal menu (see `menu_apply_by_bind()`), and can be used directly in protocol to avoid extra `switch`.  
7. **Weak callbacks.** User handlers can be defined in `src/menu/user_menu_callbacks.cpp`, while generator puts weak impls in `src/menu/menu_callbacks_weak.cpp` for linking even without impl.  

Thanks to this autogeneration, protocol doesn't manually map "which ID goes with which var": `MENU_ID` → `g_menu` index → pointer to `MenuState` field and metadata (`vtype`, `scope`, `persist`). Reverse path gets `bind` or function name for logs/telemetry.

### Example mappings
- `MENU_DRY_TEMP = 3` (`src/menu/menu_ids.h:4`). In `g_menu[3]` (`src/menu/menu_data.cpp:70`), this is `MN_VALUE`, type `VT_F32`, range `30…110`, pointer to `menu.dry_temp`, scope `SCOPE_PER_CONTROLLER`. Corresponding `g_bindings` entry (`src/menu/menu_bindings.cpp:29`) references same array and defines EEPROM offset `2221`.  
- `MENU_STORAGE_START = 10` (`src/menu/menu_ids.h:11`). In `g_menu[10]` (`src/menu/menu_data.cpp:90`), type `MN_ACTION`, function `start_storage`. No `g_bindings` entry for actions, but protocol can call function via `item.action`.  
- Any new var added to `menu_v2.yaml` auto-appears in all listed files, so protocol needs no manual sync: just use `MENU_ID` and `CTRL_INDEX`, rest pulls from autogenerated tables.

## Implementation recommendations
1. Implement shared frame serialization/deserialization library (RP2040 ↔ ESP32) with TLV type table.  
2. Add `protocol_handler.cpp` module linking messages to `g_menu`/`menu_bindings`.  
3. Document new TLV and error codes in this file so UI/servers can auto-generate client parts.  
4. Write test scanning all `MENU_*` IDs: form `SET_VALUE` frame, check corresponding `menu` field updated and EEPROM written (unit test/Host test).  
5. Extend protocol (telemetry, subscriptions) via `HELLO` (capability bitmask). Client learns available features first, then uses them.
