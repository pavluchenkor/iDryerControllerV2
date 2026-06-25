# Flash iDryer MCU via install.idryer.org

Instructions for flashing iDryer mcu directly from the browser using WebUSB.

Website: <https://install.idryer.org/>

## Before you start

- Browser: Chrome, Edge, or another Chromium-based browser with WebUSB support.
- iDryer board in BOOTSEL mode.
- USB data cable.
- Close all programs that may hold the port:
  - Arduino IDE
  - PlatformIO / Serial Monitor
  - any serial terminals
- On macOS: if `RPI-RP2` is mounted in Finder, eject it first.

## Windows: WebUSB Issues

Windows may not recognize RP2040 for WebUSB without WinUSB driver. Options:

- **Web installer:** install driver via [Zadig](https://zadig.akeo.ie/) (Options → List All Devices → select "RP2 Boot" → WinUSB → Install Driver).
- **Direct UF2 download:** click **💻 Windows Instructions** button on the website to download `.uf2` files and copy to `RPI-RP2` drive.

## Flashing steps

1. Connect iDryer via USB.
2. Put the controller into BOOTSEL mode:
   - hold `BOOTSEL` while connecting USB,
   - or hold `BOOTSEL` and press `RESET`.
3. Open <https://install.idryer.org/>, UNIT MCU section.
4. Click `Connect RP2040`.
5. In the system dialog, select your RP2040 microcontroller.
6. Click `Flash Firmware`.
7. Wait until flashing is complete.

## First flash timing note

The first flash can take longer than usual (up to 1-1.5 minutes), because a large EEPROM/service-data update is performed.

## If something goes wrong

### Flash error (`Failed to claim interface`)

1. Fully close PlatformIO Serial Monitor, Arduino IDE, and other serial programs.
2. Disconnect the device.
3. Hold `BOOTSEL` and reconnect USB.
4. On macOS, eject `RPI-RP2` if it appears.
5. Try a different USB port or cable.

### Massive errors about non-existent modules / menu lockup

This exact scenario is what `Erase Firmware` is for.

Typical signs:

- wrong port modes were selected by mistake (for example, all ports set to `EXT`);
- modules were enabled that are not physically present (screen, Link, extra Unit, etc.);
- device keeps producing errors for a non-existent module;
- due to repeated errors, entering menu and restoring config becomes impossible.

Why it happens:

- port/module configuration is stored in flash/EEPROM;
- after saving an incorrect configuration, device can keep booting into that broken state.

What to do:

1. On <https://install.idryer.org/>, click `Erase Firmware`.
2. Wait for full flash erase.
3. Immediately run `Flash Firmware`.
4. After first boot, reconfigure ports and modules correctly.

## Recommendation after flashing

After successful flashing, let the controller finish the first startup and initialization completely. Do not disconnect power or USB too early.
