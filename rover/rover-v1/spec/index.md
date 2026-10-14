# Rover C3: spec index

> **This file is the router, not the spec.** Each subject owns one document. Open the one you
> need; you should not have to read anything else.
>
> **Last updated:** 2026-10-14 · **Protocol:** v1 · **Firmware:** 0.1.0 (`firmware/`) ·
> **App:** not started (M4, [app.md](features/app.md)) · **Target:** ESP32-C3 SuperMini or Seeed
> XIAO ESP32C3 · PlatformIO, espressif32 7.1.3 (Arduino core 2.0.17), NimBLE-Arduino 2.5.1.

---

## The documents

Four folders, by what a document is *for*. A new document goes in one of them and gets a row
here.

### `core/`: how the system is shaped, across every feature

| Doc | Owns |
|---|---|
| [architecture.md](core/architecture.md) | How phone, ESP32 and motors fit; the modules and their contracts; the **threading rule**; the pure C++ core; where tunables live; dependency rule; risks |
| [protocol.md](core/protocol.md) | **The contract**: NUS UUIDs, every packet, the checksum, the test vectors, the lines the rover sends, the protocol version. Change it here first |
| [hardware.md](core/hardware.md) | The modules, the **pin map**, the rev C test-harness wiring, power, **bench rules**, wiring for later items |
| [build.md](core/build.md) | Toolchain and versions, VS Code, commands, unit tests on the PC, gotchas, **definition of done** |
| [guards.md](core/guards.md) | What `tool/guard.py` checks and why; commit rules. **Run it before every commit** |

### `features/`: one per feature

| Doc | Owns |
|---|---|
| [drive.md](features/drive.md) | Who drives (pad, joystick, bench), the Control Pad table, speed levels, the ramp, motor output, the **failsafes** |
| [link.md](features/link.md) | Advertising, one phone at a time, connect and disconnect, the hello, the status line's timing, the UART echo |
| [serial.md](features/serial.md) | USB serial: the boot line, the heartbeat, the bench commands |
| [app.md](features/app.md) | The phone app (M4): what any app must do, and the open decision on how to build it |

### `guides/`: procedures

| Doc | Owns |
|---|---|
| [bluefruit-test.md](guides/bluefruit-test.md) | Testing by hand with Bluefruit Connect (pad, UART echo) and nRF Connect (joystick packets, the timeout) |
| [tuning.md](guides/tuning.md) | M3 floor tuning: directions, `MIN_DUTY`, `MAX_DUTY`, `RAMP_MS` |
| [troubleshooting.md](guides/troubleshooting.md) | Symptom → likely cause → fix |

### `status/`: where things stand

| Doc | Owns |
|---|---|
| [roadmap.md](status/roadmap.md) | Milestones and items, order of work, status, later upgrades. **Item numbers are permanent** |
| [testing.md](status/testing.md) | Hardware checklists per milestone; tuned values |
| [log.md](status/log.md) | **Dated work log, newest first**: one entry per session (built / decided / tested / next) |
| [handover.md](status/handover.md) | **Where the last session left off**: what's untested, the owner's next steps, open decisions |

### At the root

- [product.md](product.md): what it is, who it's for, confirmed and open decisions, short user
  guide.
- [code-map.md](code-map.md): **where** every file is and what's in it, plus a "task → file"
  table. An index, not a spec: it never says *why*.
- [ROVER_PLAN.md](../ROVER_PLAN.md): the owner's original plan (2026-10-14), kept as written.
  `spec/` took it over; where they differ, `spec/` wins. The differences are listed in
  [log.md](status/log.md#2026-10-14-the-c3-firmware-m0m2-and-the-spec).

## Where each cross-cutting rule lives

| Rule | Home |
|---|---|
| The protocol is the contract: edit it first, bump the version, then firmware and app together | [protocol.md](core/protocol.md) |
| Every pin, limit, timing and ID lives in `firmware/include/config.h` | [architecture.md](core/architecture.md#tunables) |
| Bluetooth callbacks never touch the motors: they queue bytes and set flags for `loop()` | [architecture.md](core/architecture.md#threading) |
| `lib/RoverCore` is pure C++ (no Arduino, no NimBLE), takes tunables as parameters, and is unit-tested on the PC | [architecture.md](core/architecture.md#rovercore-the-pure-c-core) |
| IN1–IN4 LOW first thing in `setup()`; a disconnect or a joystick timeout stops at once, without the ramp | [drive.md](features/drive.md#failsafes) |
| Pull W5 whenever the ESP32 is on USB; tracks lifted for every first test | [hardware.md](core/hardware.md#bench-rules) |
| Build only what the roadmap's current milestone lists; finish each on real hardware | [roadmap.md](status/roadmap.md) |
| Every add / delete / rename of a source file updates the code map | [guards.md](core/guards.md) |
| No AI co-author trailers; the owner commits | [guards.md](core/guards.md#commit-rules) |
| Add a log entry at the end of every session | [log.md](status/log.md) |

## Writing rule

- **One subject, one document.** A rule is stated once, in its home above, and linked from
  everywhere else.
- Each doc opens with **Owns / Does not own**.
- Dates are absolute (`2026-10-14`), never "yesterday".
