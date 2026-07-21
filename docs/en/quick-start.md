# Quick Start

A short route from printing the parts to a working dryer: printing the case,
wiring the modules, flashing, claiming on the portal, and initial damper setup.
Details for each step are in the dedicated documentation sections.

## What you need

- Controller board (RP2040) and the **iDryer-Link** module (ESP32-C3 Super Mini).
- A data USB cable.
- A Chromium browser (Chrome, Edge) with WebUSB support.
- A 2.4 GHz Wi-Fi network with a password.
- An account on the portal — <https://portal.idryer.org>.

## 1. Wiring

!!! warning "Test the assembly on the bench first"
    Before final assembly, put all components together on the bench and make
    sure the device works. Wiring mistakes are easier to find while you still
    have access to every part.

!!! danger "Power"
    Never connect or disconnect modules (Link, screen, sensors) while power is
    applied. Do all wiring with the device powered off.

Wire the modules and components according to the dedicated documentation
section. Link must be connected to the controller before flashing.

!!! warning "Do not swap wires"
    Wiring looks simple, but wires to the board are often swapped. Such mistakes
    are hard to diagnose remotely: the dryer appears to run normally, but the
    control logic changes. For example, if the fan and heater are swapped, the
    device seems to work, but the PID controller does not control the heater —
    the heater runs at full power all the time.

## 2. Flash the controller and Link

Flashing is done from a browser at <https://install.idryer.org>. Follow the
wizard steps in order:

1. **Flash Controller** — connect USB to the controller port, put the board into
   `BOOTSEL` mode, and flash the controller.
2. **Flash Link** — move the USB cable to the Link port (Link stays connected to
   the controller) and flash the module.

## 3. Wi-Fi and claiming on the portal

Continue in the same wizard at <https://install.idryer.org>:

1. **Wi-Fi** — after flashing Link, the network setup wizard (Improv) opens.
   Enter your Wi-Fi network name (SSID) and password.
2. **Claim** — start the claim procedure. The wizard shows a `PIN`.
3. **Portal** — open <https://portal.idryer.org>, sign in, add a device on the
   devices page, and enter the `PIN`.

After claiming, the device appears in the list on the portal.

## 4. Printing the case parts

Print the case parts with the settings specified in the CAD section of the
documentation. These settings have been proven across thousands of builds. If
you deviate from them, the case loses thermal insulation and the dryer does not
reach its operating temperature.

## 5. Damper and servo

You can set up the damper from the controller screen (menu `SETTINGS → SERVO`),
from the device settings on the portal, or from the app.

!!! warning "Damper installation order"
    Set the angle first, then install the damper — otherwise it will hit the
    case and stall the servo.

1. Set `CLOSED ANGLE = 0`. The servo moves to this position (preview).
2. Based on the actual shaft position, install the damper so that in the closed
   position it fully blocks <!-- TODO: confirm term — opening/shaft/channel of
   the damper assembly --> the air channel of the damper assembly.
3. Set `OPEN ANGLE` to match your mechanics. This step can also be done after
   final assembly.

## 6. Heater PID controller

The firmware already ships with working PID controller values — no separate
calibration is required to start and perform the initial check. If needed, run
autotune to fit the coefficients to your build.

## 7. Control via the portal and app

All controller functions and menus are available through the portal and the app.
The portal and app significantly extend the dryer's capabilities: telemetry,
data history, presets, and remote control.

Control is available from the portal <https://portal.idryer.org> or from the app:

- **Google Play** — <https://play.google.com/store/apps/details?id=org.idryer.mobile>
- **App Store** — <https://apps.apple.com/app/idryer/id6760609044>

To start drying:

1. Open the portal or app — the card of your device appears on the screen.
2. Select the mode — drying or storage.
3. Press start.

The default temperature and time values are chosen for most cases. Change them
to suit your material if needed.

### Filament tracking and reviews

Every filament on your shelf is reflected on the portal, and all data is
recorded. You can leave a review for each filament and read reviews from other
users. Reviews are grouped by manufacturer, type, and other attributes and are
available directly on the portal and the forum.
