# Phone app (M4)

> **Owns:** what the phone app must do, its packages and its outline.
> **Does not own:** the bytes ([protocol.md](../core/protocol.md)), the rover's behaviour
> ([drive.md](drive.md)).

---

**Status (2026-10-14):** not started. A new Flutter app (Android and iOS) in `app/` at the repo
root, next to `firmware/`. Nothing in the firmware depends on it.

## What the app must do

| Job | Rule |
|---|---|
| Find | Scan; list devices named **RoverC3** or advertising the NUS service UUID |
| Connect | Find RX and TX by UUID, subscribe to TX, read the HELLO line (`fw`, `pv`) |
| Drive | Joystick → [mixer](#mixer) → J packet every **50 ms** (20 Hz) with **write without response**. This stream feeds the rover's 300 ms failsafe |
| Release | Joystick released → `J 0 0` at once |
| Speed | Chips 40 / 70 / 100 % scale the mixer's output |
| Stop | A big STOP button: `J 0 0` |
| Leave | App paused or detached → `J 0 0`, then disconnect |
| Status | Show the latest `OK` line from TX ([protocol.md](../core/protocol.md#rover--phone-tx)); ignore lines it doesn't know |
| Tests | The packet builder against protocol.md's [worked examples](../core/protocol.md#worked-examples) |

**Done when (M4):** the app finds RoverC3, drives it with the joystick, and the rover stops
within 300 ms of closing the app or losing the connection.

### Mixer

`x`, `y` from the stick in −1..1. `y = −stickY` (screen up is negative; check on the phone).
Deadzone 0.08 on each axis. `left = clamp(y + x)`, `right = clamp(y − x)` to −1..1, times the
speed chip, rounded to an int in −100..100.

## Packages

- `flutter_blue_plus`: the Bluetooth LE central. Free for individuals and organisations under 15
  employees.
- `flutter_joystick` (0.2.x): the on-screen stick; its listener gives x / y in −1..1.

Android permissions from the `flutter_blue_plus` README: `BLUETOOTH_SCAN` with
`neverForLocation`, `BLUETOOTH_CONNECT`; location for Android 11 and lower.

## Outline

```
app/lib/
  main.dart
  protocol/packets.dart     checksum(), joystickPacket(l, r) → Uint8List
  drive/mixer.dart          (x, y) → (left, right) in −100..100
  ble/rover_link.dart       scan, connect, find RX/TX by UUID, send(), status stream
  ui/connect_screen.dart    scan list filtered by name "RoverC3" or the NUS service UUID
  ui/drive_screen.dart      joystick, speed chips 40/70/100, STOP button, status bar
```

The 20 Hz send loop is a Dart `Timer.periodic(50 ms)` that sends the latest J packet.
