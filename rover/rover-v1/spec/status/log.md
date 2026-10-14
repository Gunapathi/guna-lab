# Work log

> **Owns:** what happened when: one entry per work session, **newest first**, so progress can be
> followed over time. History only, never a live rule (rules live in their docs).
> **Format:** `## YYYY-MM-DD: headline`, then **Built**, **Decided**, **Tested**, **Next**.
> Roadmap item numbers ([roadmap.md](roadmap.md)) in brackets.

---

## 2026-10-14: the C3 firmware (M0–M2) and the spec

The owner asked for the C3 firmware from `ROVER_PLAN.md` first, with the phone app to follow in
the same folder, and the folder and spec laid out like Machi (`D:\WORK\OWN\machi`).

**Built**
- [1] The repo: `firmware/` (PlatformIO), `app/` reserved, `spec/` in Machi's shape (router,
  code map, core / features / guides / status), `CLAUDE.md`, `tool/guard.py`,
  `rover-c3.code-workspace`, `.gitignore`, `.gitattributes`.
- [2] M0: the boot line with the reset reason (BROWNOUT named), the 2 s heartbeat.
- [3] M1: `motors.cpp` (LEDC, 1 kHz, 8 bits; builds on Arduino core 2.x and 3.x), the serial
  bench commands, the ramp.
- [4] M2: `lib/RoverCore` (protocol, mixer, drive) with 23 Unity tests; the NimBLE NUS server;
  the echo; the HELLO and OK lines; the failsafes.

**Decided** (where the spec differs from `ROVER_PLAN.md`; the plan stays as written)
- **RoverCore gained `drive.h/.cpp`:** who drives, the targets, both failsafes and the ramp,
  with time passed in, so the failsafes are unit-tested. `padToTracks` takes the speed, so
  `PadState` has no level (the drive holds it). `motorDuty` moved into RoverCore. Tunables come
  in as parameters, so tuning `config.h` never breaks a test.
- **The joystick timeout stops at once, like a disconnect** (the plan ramped it): stopping coasts
  and draws no current, and M4's "stops within 300 ms" can't hold with a 300 ms ramp after a
  300 ms timeout.
- **`ble_link` queues whole writes** (a FreeRTOS queue, Machi's pattern) instead of a byte ring
  buffer, so typed text can be echoed and packets parsed. `bleSendStatus` became `bleSend`
  (HELLO, status, echo), which splits lines to the MTU. `bleTakeSubscribed` added for the hello.
- **Protocol v1 adds the HELLO line** (name, firmware, protocol version) on subscribe. The status
  line's `L`/`R` are the targets; it goes out on change at most every 200 ms, and every second.
- **The parser knows the other Bluefruit packet lengths** (C, A, G, M, L, Q) and skips them, so a
  `!` inside sensor data can't fake a packet.
- **The ramp step is rounded up** (4 % a tick, 250 ms to full) so a full ramp fits `RAMP_MS`.
- **The speed starts at the slowest level (40 %).**
- **`platformio.ini`:** shared settings in an `[esp32c3]` section (the plan's `[env]` would have
  given the native env `framework = arduino`); versions pinned (`espressif32@7.1.3`,
  `NimBLE-Arduino@2.5.1`); `test_ignore` on the boards; `-Wall -Wextra` on `src/`; the native
  env uses the board's `-std=gnu++11`.
- **`config.h` said "tune in M5":** the plan's tuning is M3; fixed.
- **Bench rule added:** switch SW1 off before unplugging USB (the L298N's inputs float).
- **The app:** two options written up with a recommendation (A, a new Flutter app with Machi's
  look); the owner decides ([app.md](../features/app.md#the-decision)).
- **Later the same day:** PlatformIO IDE 3.3.4 finds a project only at a workspace folder's top,
  so the workspace file is required (opened for the owner). PIO Home no longer opens at startup
  (`platformio-ide.disablePIOHomeStartup` in the workspace). The owner asked about building from
  the Arduino IDE instead, then chose to stay on PlatformIO.
- **The owner's board is the XIAO ESP32C3** (no SuperMini): `default_envs = xiao`; the XIAO's pad
  map (GPIO4–7 = D2–D5) and its warnings (5V pad on USB 5V, BAT pads, antenna) in
  [hardware.md](../core/hardware.md#xiao-esp32c3-pads). M0's heartbeat seen on the XIAO (COM8).

**Tested**
- PlatformIO Core 6.2.0 installed into `%USERPROFILE%\.platformio\penv` (it wasn't on the PC).
  `espressif32` 7.1.3 turned out to still ship Arduino core 2.0.17.
- `pio run`: SuperMini 502,214 bytes, XIAO 491,810 (38 %), no warnings with `-Wall -Wextra`.
- The same sources on Arduino core 3.3.1 (Machi's arduino-cli, XIAO_ESP32C3, `--warnings all`):
  builds, 632,958 bytes. That covers the core 3.x branch in `motors.cpp`.
- `pio test -e native`: 23/23 pass, with PlatformIO's MinGW package (GCC 5.1) from a temporary
  folder. That run found a real portability bug: `snprintf`'s return value on a cut line. Fixed
  in `protocol.cpp`.
- Not on hardware: everything in [testing.md](testing.md).

**Next**
- The owner: install the PlatformIO IDE extension, open `rover-c3.code-workspace`, then the
  hardware pass M0 → M1 → M2 ([testing.md](testing.md)).
- The owner: decide how to build the app ([app.md](../features/app.md#the-decision)).
