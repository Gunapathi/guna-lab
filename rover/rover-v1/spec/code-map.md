# Code map

> **Purpose:** a routing table, not a spec. It says **where** code is; the *why* is in the docs it
> links to. **Update it on every add, delete, move or rename of a source file**
> (`tool/guard.py` step 1 blocks otherwise).

---

## 1. Layout at a glance

```
<repo root>/
  CLAUDE.md                 rules for agents; points here and to spec/index.md
  ROVER_PLAN.md             the owner's original plan, kept as written (spec/ took it over)
  rover-c3.code-workspace   VS Code: the root + firmware/ (+ app/ later), so PlatformIO finds its project
  spec/                     the docs: index.md routes to everything
  tool/guard.py             run before every commit (spec/core/guards.md)
  firmware/                 PlatformIO project for the ESP32-C3 (C++, Arduino core 2.x)
    platformio.ini            envs: supermini, xiao, native (unit tests); pinned versions
    include/config.h          every pin, limit, timing and ID
    lib/RoverCore/            pure C++: protocol, mixer, drive (tested on the PC)
    src/                      the Arduino side: main, motors, ble_link, serial_cmd
    test/test_core/           Unity tests for RoverCore
  app/                      the phone app (M4, not started: spec/features/app.md)
```

## 2. Firmware: `firmware/`

Rules: [architecture.md](core/architecture.md), [protocol.md](core/protocol.md),
[drive.md](features/drive.md).

| File | Contents | Used by |
|---|---|---|
| [platformio.ini](../firmware/platformio.ini) | `[esp32c3]` shared settings (platform and NimBLE pinned, USB CDC flags, `-Wall -Wextra` on `src/`, `test_ignore`), envs `supermini` (`esp32-c3-devkitm-1`), `xiao` (`seeed_xiao_esp32c3`), `native` (Unity) | PlatformIO |
| [config.h](../firmware/include/config.h) | **Every** tunable: `FW_VERSION`, `PROTOCOL_VERSION`, `PIN_IN1`–`PIN_IN4`, PWM, `MIN_DUTY` / `MAX_DUTY`, `INVERT_*`, `SPEED_LEVELS_PCT`, `START_LEVEL`, `RAMP_MS`, `CONTROL_TICK_MS`, `SLEW_STEP_PCT`, `JOY_TIMEOUT_MS`, `DEVICE_NAME`, the NUS UUIDs, `BLE_*`, `STATUS_*`, serial timings | every `src/` file |
| [protocol.h/.cpp](../firmware/lib/RoverCore/protocol.cpp) | `Packet`, `checksum`, `PacketParser` (`feed`, `busy`, `reset`; `packetLength`: B, J and the other Bluefruit types to skip), `formatHello`, `formatStatus` | main, drive, tests |
| [mixer.h/.cpp](../firmware/lib/RoverCore/mixer.cpp) | `PadState`, `padToTracks`, `slew`, `motorDuty` | drive, motors, tests |
| [drive.h/.cpp](../firmware/lib/RoverCore/drive.cpp) | `DriveConfig`; `Drive`: `packet` (→ `button`, `joystick`), `bench`, `linkLost`, `tick` (joystick timeout, then the ramp), `left` / `right` (outputs), `targetLeft` / `targetRight`, `level`; `NOTHING_HELD`, `clampPct` | main, serial_cmd, tests |
| [main.cpp](../firmware/src/main.cpp) | `setup` (motors LOW first, serial with a non-blocking TX, the boot line with `resetReason`, `bleBegin`); `loop`: `checkLink` (the disconnect failsafe), `readPhone` (echo text, parse packets), `serialPoll`, the control tick, `sendStatus` (hello + status), `heartbeat` | entry point |
| [motors.h/.cpp](../firmware/src/motors.cpp) | `motorsBegin`, `motorsSet`; `pwmAttach` / `pwmWrite` (Arduino core 2.x or 3.x), `setMotor` | main |
| [ble_link.h/.cpp](../firmware/src/ble_link.cpp) | NimBLE NUS server: `bleBegin` (service, advertising, `advertiseOnDisconnect`), `bleConnected`, `bleRead` (one queued write), `bleTakeSubscribed`, `bleSend` (split to the MTU); `ServerCallbacks`, `RxCallbacks` (`Write` queue), `TxCallbacks` (subscribe flag) | main |
| [serial_cmd.h/.cpp](../firmware/src/serial_cmd.cpp) | `serialPoll` (bench commands → `Drive::bench`), `printHelp` | main |
| [test_main.cpp](../firmware/test/test_core/test_main.cpp) | Unity tests: checksum and parser on the protocol vectors, resync, skipping other Bluefruit packets, the status lines, the pad table, slew, duty, and `Drive` (ramp, levels, stop button, joystick timeout, link lost, clamping, bench) | `pio test -e native` |

## 3. Tools

| File | Contents |
|---|---|
| [guard.py](../tool/guard.py) | The pre-commit checks ([guards.md](core/guards.md)): code map, doc links, protocol constants, RoverCore purity, AI trailers, firmware build, unit tests |
| [rover-c3.code-workspace](../rover-c3.code-workspace) | VS Code workspace folders ([build.md](core/build.md#vs-code)) |

## 4. Task → file routing

| I need to… | Start here |
|---|---|
| Change a pin, limit, timing or the Bluetooth name | `firmware/include/config.h` only |
| Tune speeds, the ramp or motor directions | `config.h` → [tuning.md](guides/tuning.md) → record the values in [testing.md](status/testing.md) |
| Add or change a packet | [protocol.md](core/protocol.md) first → `protocol.cpp` (`packetLength`, `feed`) → `Drive::packet` → bump `PROTOCOL_VERSION` → tests |
| Change how the Control Pad mixes | `padToTracks` (`mixer.cpp`) → the table in [drive.md](features/drive.md#control-pad) → `test_pad_table` |
| Change a failsafe | `Drive::tick` / `Drive::linkLost` (`drive.cpp`), `checkLink` (`main.cpp`) → [drive.md](features/drive.md#failsafes) → the `Drive` tests |
| Change the status or hello line | [protocol.md](core/protocol.md#rover--phone-tx) → `formatStatus` / `formatHello` → `test_status_lines` (the app parses these) |
| Change when the status is sent | `sendStatus` (`main.cpp`), `STATUS_MS` / `STATUS_MIN_MS` → [link.md](features/link.md#status-line) |
| Add a serial command | `Drive::bench` (a drive command) or `serialPoll` (anything else) → [serial.md](features/serial.md) |
| Add a board | an env in `platformio.ini` → [hardware.md](core/hardware.md#pin-map) → [build.md](core/build.md) |
| Battery voltage, buzzer (later) | [hardware.md](core/hardware.md#later-wiring) → `config.h` → a new `src/` module → this map |
