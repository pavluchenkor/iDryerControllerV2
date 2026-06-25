# pn532

Local fork of `Seeed-Studio/PN532` for the iDryer RP2040 firmware.

Goals of this fork:

- keep the original Arduino-style API working;
- expose SPI `ready` state to the application;
- support split passive-target detection flow:
  `start -> IRQ/ready -> read result`.

The existing blocking API remains available:

```cpp
bool found = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLen, 100);
```

New interrupt-friendly flow:

```cpp
volatile bool pn532_irq = false;

void onPn532Irq() {
  pn532_irq = true;
}

if (!nfc.passiveTargetIDDetectionPending()) {
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

if (pn532_irq) {
  pn532_irq = false;

  if (nfc.isReady()) {
    if (nfc.readDetectedPassiveTargetID(uid, &uidLen, 1)) {
      // tag read successfully
    }
  }
}
```

Notes:

- ISR must only set a flag. Do not call SPI or PN532 methods from ISR.
- `startPassiveTargetIDDetection()` still waits for `ACK` inside `writeCommand()`.
  The long wait for tag/response is what gets moved out of the blocking call.
- This fork currently exposes split detection for the SPI transport, which is
  the transport used by this project.

## Current State In iDryer

This fork was integrated and tested in the iDryer RP2040 project with:

- `RP2040 + earlephilhower`
- `SPI`
- one `PN532`
- one `NTAG215`
- IRQ-assisted detect on GPIO

What is confirmed to work:

- non-blocking passive tag detection in the main loop;
- stable `TagDetected` / debounced `TagRemoved`;
- fast UID detection without long loop stalls;
- separated flows for:
  - tag ID detection
  - content read
  - content write
  - readback verify
- synchronous and asynchronous higher-level transfer strategies in the project code.

## Observed Timings

Measured on the current hardware during project bring-up:

- UID detect: about `23..30 ms`
- preview read (`32 bytes`): about `94..95 ms`
- test write (`308 bytes` user area on `NTAG215`): about `500 ms` when successful
- test readback (`308 bytes`): about `900 ms` when successful
- worst single page step during write/read: about `10..13 ms`

These numbers are good enough for:

- tag presence detection;
- reading tag ID for portal integration;
- occasional service writes with explicit verify.

These numbers are not ideal for:

- aggressive runtime rewrites on every small process update;
- guaranteed hard real-time update windows while the tag is moving quickly.

## Important Write Limitation

Write support on `PN532 + NTAG215` is usable, but not perfectly deterministic.

Observed behavior during repeated tests:

- some write cycles complete successfully;
- some write cycles fail immediately with PN532 status `0x27`;
- successful write cycles can be verified by full readback;
- failed write cycles are now reported honestly by this fork and are no longer treated as false success.

This means:

- write on this stack is **possible**;
- write on this stack is **not strong enough to be treated as guaranteed production-grade every time**.

In practice, any write operation must use:

- retry policy;
- readback verify;
- failure reporting to upper layers.

## Suitability For This Project

This PN532 stack is suitable for:

- portal-facing tag detection;
- reporting UID and basic tag metadata;
- reading existing OpenPrintTag-like payload from `NTAG21x`;
- writing small or moderate amounts of service data when retries and verify are acceptable;
- bringing up project architecture before moving to a different NFC frontend.

This PN532 stack is not the final target for:

- full `OpenPrintTag` specification compatibility;
- `ISO15693 / NFC-V / SLIX2`;
- iPhone-oriented spec-compatible production implementation.

For that direction, the project should move to `PN5180`.

## Practical Conclusion

For iDryer, this fork should be treated as:

- a valid development and integration platform;
- a working detector/reader;
- a conditionally usable writer with mandatory verify;
- not the final production RFID/NFC solution for spec-compatible `OpenPrintTag`.
