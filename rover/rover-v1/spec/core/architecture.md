# Architecture

> **Owns:** how phone, ESP32 and motors fit; the firmware modules and their contracts; the
> threading rule; the pure C++ core; where tunables live; the dependency rule; risks.
> **Does not own:** byte formats ([protocol.md](protocol.md)), wiring and pins
> ([hardware.md](hardware.md)), drive behaviour ([drive.md](../features/drive.md)), file
> locations ([code-map.md](../code-map.md)).

---

## How it fits together

```
 PHONE                          ROVER: ESP32-C3                                  L298N + 2 TT MOTORS
 ┌─────────────────────────┐    ┌────────────────────────────────────────────┐   ┌──────────────────┐
 │ Bluefruit Connect        │ RX │ ble_link (NimBLE task)                     │   │                  │
 │   Control Pad: !B        │ ─► │   └─ write queue ─┐                        │   │ OUT1/2 ─ left    │
 │ Own app (M4)             │    │ loop():           ▼                        │PWM│ OUT3/4 ─ right   │
 │   joystick: !J at 20 Hz  │    │   PacketParser → Drive → ramp → motors ────┼──►│                  │
 │   status bar             │ ◄─ │   status line         ▲                    │IN1│ +12V ← SW1 ← BT1 │
 └─────────────────────────┘ TX │   serial_cmd (USB) ───┘ failsafes          │-4 │ +5V ─ W5 → ESP32 │
                                └────────────────────────────────────────────┘   └──────────────────┘
```

- The **phone decides** where to go: the Control Pad sends button presses, the own app sends
  final track speeds.
- The **rover keeps itself safe**: it ramps every change, and stops by itself when the link
  drops or the joystick stream stops ([drive.md](../features/drive.md#failsafes)).
- There is no pairing yet: any phone in range can connect while the rover advertises (one at a
  time; [link.md](../features/link.md)).

## Data flow

```
Phone ──BLE write──► ble_link (NimBLE task)
                       │ copy the write into a queue (no motor calls here)
                       ▼
loop(): next write ─┬─ starts with '!' → PacketParser → Packet → Drive::packet
                    │     ├─ 'B' button   → held directions + level → target L/R %
                    │     └─ 'J' joystick → target L/R %
                    └─ text → echoed on TX (the link check)
        serialPoll() → Drive::bench (f b l r s 1 2 3)
        every CONTROL_TICK_MS: Drive::tick (joystick timeout, then the ramp) → motorsSet(L, R)
        link lost: Drive::linkLost → motorsSet(0, 0) at once
        status line on TX: on change, and every second
```

## Modules

| Module | Where | API | Behaviour |
|---|---|---|---|
| `protocol` | `lib/RoverCore` | `checksum(p, n)`; `PacketParser::feed(b, out)`, `busy()`, `reset()`; `formatHello`, `formatStatus` | Byte-by-byte state machine ([protocol.md](protocol.md#parsing)). Emits B and J packets; reads the other Bluefruit packets to their end and drops them |
| `mixer` | `lib/RoverCore` | `padToTracks(pad, s, l, r)`; `slew(current, target, maxStep)`; `motorDuty(pct, minDuty, maxDuty)` | The Control Pad table, the ramp step, and % → PWM duty ([drive.md](../features/drive.md)) |
| `drive` | `lib/RoverCore` | `Drive(cfg)`; `packet(p, now)`, `bench(cmd)`, `linkLost()`, `tick(now)`; `left()`, `right()`, `targetLeft()`, `targetRight()`, `level()` | Who drives (pad or joystick), the targets, the failsafes and the ramp. Time comes in as `now` |
| `motors` | `src` | `motorsBegin()`, `motorsSet(leftPct, rightPct)` | L298N through LEDC PWM, −100..100 per track, sign picks the pin, 0 coasts, `INVERT_*` applied. `motorsBegin()` drives all four pins LOW first |
| `ble_link` | `src` | `bleBegin()`, `bleConnected()`, `bleRead(buf, max)`, `bleTakeSubscribed()`, `bleSend(line)` | NUS server; queues each write whole; restarts advertising on disconnect; splits sent text to the MTU |
| `serial_cmd` | `src` | `serialPoll(Drive&)` | Single-letter bench commands ([serial.md](../features/serial.md)) |
| `main` | `src` | `setup()`, `loop()` | Wires the modules together; owns the disconnect failsafe's edge, the hello and the status timing |

## RoverCore: the pure C++ core

`firmware/lib/RoverCore/` holds every piece of logic that decides what the motors do: packet
parsing, the pad table, the ramp, the duty mapping, the modes and the failsafes. It follows three
rules, so it can be unit-tested on the PC (`pio test -e native`, [build.md](build.md#unit-tests-on-the-pc)):

- **No Arduino, NimBLE or ESP-IDF headers**: only the C/C++ standard library (the guard checks
  it, [guards.md](guards.md)).
- **Tunables come in as parameters** (`DriveConfig`, `padToTracks(…, s, …)`,
  `motorDuty(…, minDuty, maxDuty)`), never from `config.h`, so tuning the rover never breaks a
  test.
- **Time comes in as a parameter** (`now`), so a test can step it.

`src/` is the thin Arduino side: it reads the hardware and the radio, calls RoverCore, and writes
the pins.

## Threading

NimBLE runs its callbacks on its own FreeRTOS task. **Those callbacks never touch the motors.**
They copy each write into a FreeRTOS queue (`BLE_WRITE_QUEUE` writes of up to `BLE_WRITE_MAX`
bytes; a full queue drops the write rather than stall the Bluetooth task) or set a `volatile`
flag (connected, subscribed). `loop()` drains the queue, parses, decides and writes the pins.
Same pattern as Machi's `ble_link.cpp`.

`loop()` never blocks: no `delay()` after `setup()`, serial printing can't stall
([build.md](build.md#gotchas)), and the control tick runs every `CONTROL_TICK_MS`.

## Tunables

Every pin, limit, timing, ID and version is a named constant in `firmware/include/config.h`,
never a number inline. `main.cpp` hands the ones RoverCore needs to it through `DriveConfig`.

## Keep it light (dependency rule)

- **Firmware** uses only **NimBLE-Arduino** plus what the ESP32 Arduino core ships (`Arduino.h`,
  LEDC, FreeRTOS queues, `esp_reset_reason`).
- **Tests** add only **Unity**, which PlatformIO brings for `test_framework = unity`.
- **The app** is not built yet; its dependencies are part of the open decision in
  [app.md](../features/app.md#the-decision).

## Code style

- `// ---------- Section ----------` headers, comments that say *why*, not *what*.
- Firmware files are small and single-purpose; a new module gets a `.h/.cpp` pair and a code-map
  row.
- C++11 or later only where the native build and the ESP32 build both take it (the ESP32 build
  is GCC 8.4, `gnu++11` by default on Arduino core 2.x).

## Risks

| Risk | Handling |
|---|---|
| The ESP32 resets when motors start or reverse (voltage dip) | The ramp (`RAMP_MS`), `MAX_DUTY` ≤ 230, a shared ground (W6); the boot line names a `BROWNOUT` ([troubleshooting.md](../guides/troubleshooting.md)) |
| The phone goes out of range without disconnecting | Joystick mode stops after `JOY_TIMEOUT_MS`; the link's own supervision timeout ends the connection later |
| A held Control Pad button and a lost release | A disconnect clears every held direction; button **4** stops |
| Any phone nearby can connect and drive | One connection at a time; pairing is a later item ([roadmap.md](../status/roadmap.md)) |
| Printing over USB serial blocks on the battery (no host) | TX timeout 0: the output is dropped instead ([build.md](build.md#gotchas)) |
| A Bluefruit sensor stream fills the link | The parser skips those packets by length; the write queue drops overflow |
