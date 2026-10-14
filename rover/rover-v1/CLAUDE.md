# Rover C3

**Rover C3**: a Bluetooth-controlled tracked rover. An ESP32-C3 (SuperMini or Seeed XIAO) drives
an L298N and 2 TT motors, steered from a phone: Bluefruit Connect now, the rover's own app later
(M4). Firmware and app live in this one folder, laid out like the owner's Machi project
(`D:\WORK\OWN\machi`).

- **[spec/index.md](spec/index.md) is the router**: one document per subject, plus a table
  saying which document owns each rule. Open the one you need.
- **Before grepping, read [spec/code-map.md](spec/code-map.md)**. Update it whenever you
  add, delete, move or rename a source file (the guard blocks otherwise).
- [ROVER_PLAN.md](ROVER_PLAN.md) is the owner's original plan, kept as written; `spec/` took it
  over and wins where they differ.

## Rules you must not break

- **Safety first:** IN1–IN4 LOW first thing in `setup()`; a disconnect and a joystick timeout
  stop the motors at once. Never weaken a failsafe without the owner
  ([drive.md](spec/features/drive.md#failsafes)).
- **Protocol:** [spec/core/protocol.md](spec/core/protocol.md) is the contract. Change it
  there first, bump the version, then firmware and app together.
- **Threading:** Bluetooth callbacks only queue bytes and set flags; everything else happens in
  `loop()`. They never touch the motors.
- **RoverCore stays pure:** `firmware/lib/RoverCore` has no Arduino, NimBLE or ESP-IDF headers
  and no `config.h`; tunables and time come in as parameters; a change there gets a unit test.
- **Tunables:** every pin, limit, timing and ID lives in `firmware/include/config.h`, never
  inline.
- **Keep it light:** the firmware uses only NimBLE-Arduino plus what the ESP32 Arduino core
  ships.
- **Both builds stay green:** the boards (Arduino core 2.x, `gnu++11`) and the native tests.
- **Build only what the roadmap's current milestone lists**
  ([spec/status/roadmap.md](spec/status/roadmap.md)); finish each on real hardware.
- **The app (M4) waits for the owner's decision**
  ([spec/features/app.md](spec/features/app.md#the-decision)). Machi may be read and copied
  from, never modified from here.

## Before handing back

1. `python tool/guard.py` (or `--fast` while iterating). Details:
   [spec/core/guards.md](spec/core/guards.md).
2. **Never commit, and never add `Co-Authored-By` or any AI trailer.** The owner commits.
3. Add today's entry at the top of [spec/status/log.md](spec/status/log.md).
