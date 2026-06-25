# iDryerRP2040 Port Configuration


## Where to configure

`MENU -> GLOBAL -> PORT CONFIG`

- `PORT 1`
- `PORT 2`
- `PORT 3`

Mode values:

- `0 = EXT` — additional dryer module
- `1 = SCR` — screen
- `2 = SCL` — scales (HX711)
- `3 = LNK` — Link (UART: WiFi/Touch/portal)

## Default configuration

For initial setup and wiring, it is convenient to use the default baseline configuration:

- `U4 (PORT 3)` -> `SCR` — screen
- `U3 (PORT 2)` -> `LNK` — Link
- `U2 (PORT 1)` -> `SCL` — scales

It is important to understand the naming:

- `U2`, `U3`, `U4` are the connector labels on the enclosure/board that the user sees during wiring.
- `PORT 1`, `PORT 2`, `PORT 3` are the internal port names used in the firmware menu and in the documentation.

The mapping is:

- `U2 = PORT 1`
- `U3 = PORT 2`
- `U4 = PORT 3`

This configuration is used as the basic starting layout:

- Link is usually connected to `U3 (PORT 2)`, and this is enough for initial device setup and portal access.
- A screen on `U4 (PORT 3)` is useful for local control, but it is not required when Link is used.
- Scales on `U2 (PORT 1)` are also optional and should be connected only if needed.

## Important behavior

Changing `PORT CONFIG` does not switch hardware roles instantly.

Firmware snapshots port configuration at startup, and runtime works from that snapshot until the next reboot.

Result: after changing `PORT CONFIG`, reboot the device.

## Code-level constraints

Validation in UI and port logic allows only the following rules:

1. `PORT 1`: only `EXT` or `SCL`.
2. `PORT 2`: only `EXT`, `SCL`, or `LNK`.
3. `PORT 3`: only `SCR` or `LNK`.
4. Roles `SCR`, `SCL`, `LNK` are unique (only one port can have each role).
5. `PORT 2 = EXT` is allowed only if `PORT 1 = EXT`.

## All valid combinations

Below is the full list of valid configurations that match current rules.

| # | PORT 1 | PORT 2 | PORT 3 | Screen | Link | Scales | Max `UNITS` |
|---|---|---|---|---|---|---|---:|
| 1 | EXT | EXT | SCR | Yes | No | No | 3 |
| 2 | EXT | SCL | SCR | Yes | No | Port2 | 2 |
| 3 | EXT | LNK | SCR | Yes | Port2 | No | 2 |
| 4 | SCL | LNK | SCR | Yes | Port2 | Port1 | 1 |
| 5 | EXT | EXT | LNK | No | Port3 | No | 3 |
| 6 | EXT | SCL | LNK | No | Port3 | Port2 | 2 |

`UNITS` note:

- `UNITS` is derived from the `EXT` chain:
  - base is always 1 (built-in module);
  - if `PORT1=EXT` -> up to 2;
  - if `PORT2=EXT` as well -> up to 3.

## Practical meaning

- If you need a local screen, `PORT 3` must be `SCR`.
- If Link is on `PORT 3`, screen is unavailable on that port.
- Scales can be connected only where the port mode is `SCL` (Port1 or Port2).
- Invalid combinations are usually blocked in UI during editing.

## Recommended scenarios

1. **Screen + Link + 1 module + scales**
   `PORT1=SCL, PORT2=LNK, PORT3=SCR` (max `UNITS=1`).

2. **Screen + 2 modules + scales**
   `PORT1=EXT, PORT2=SCL, PORT3=SCR` (max `UNITS=2`).

3. **No screen, Link-focused + 2 modules + scales**
   `PORT1=EXT, PORT2=SCL, PORT3=LNK` (max `UNITS=2`).

4. **Maximum modules (3), no scales**
   - with screen: `PORT1=EXT, PORT2=EXT, PORT3=SCR`
   - with Link: `PORT1=EXT, PORT2=EXT, PORT3=LNK`

## If configuration is broken

If a non-working configuration was saved by mistake (mass module errors, menu lockup), use:

- `install.idryer.org -> Erase Firmware`
- then immediately `Flash Firmware`
- then set correct `PORT CONFIG` again
