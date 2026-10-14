# Phone app (M4)

> **Owns:** what any phone app for the rover must do, the open decision on how to build it, and
> the outline for each option.
> **Does not own:** the bytes ([protocol.md](../core/protocol.md)), the rover's behaviour
> ([drive.md](drive.md)).

---

**Status (2026-10-14):** not started. The owner decides how to build it: a new Flutter app, or
from Machi's existing app ([below](#the-decision)). Either way it lives in `app/` at the repo
root, next to `firmware/`, and nothing in the firmware depends on the choice.

## What any app must do

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

## The decision

| | **A: a new Flutter app** (the plan) | **B: start from Machi's app** |
|---|---|---|
| Shape | Flutter only: `flutter_blue_plus` (BLE) + `flutter_joystick` | Flutter UI with no runtime packages + a Kotlin core that alone touches Bluetooth |
| Size | About 6 small Dart files | Machi's app minus what a rover doesn't use (notifications, tools, games, updates, mirror…), plus a NUS class in Kotlin and a hand-made joystick widget |
| Platforms | Android and iOS | Android only |
| Runs with the app closed | No; not needed, you drive with it open | Yes: Machi's main reason for the Kotlin core, a job a remote doesn't have |
| The 20 Hz send loop | A Dart `Timer` | A Kotlin coroutine, off the UI thread |
| Proven parts | `flutter_blue_plus` is widely used | Machi's link code (reconnect, Android's error-133 back-off, one GATT operation at a time), its design system and guard |
| Licence | `flutter_blue_plus`: free for individuals and organisations under 15 employees | Machi's own code |

**Recommendation (agent, 2026-10-14): A, with Machi's look.** Build a new Flutter app as the plan
says, and copy Machi's `app/lib/core/theme/` (palette, tokens) and `core/ui/` widgets in as its
design system. A remote is used with the app open, which is what A does well. B's Kotlin core
exists for jobs that run with the app closed, and the rover has none. Pick B for one codebase
across both products, or for Machi's hardened Android link handling.

### Outline if A

```
app/lib/
  main.dart
  protocol/packets.dart     checksum(), joystickPacket(l, r) → Uint8List
  drive/mixer.dart          (x, y) → (left, right) in −100..100
  ble/rover_link.dart       scan, connect, find RX/TX by UUID, send(), status stream
  ui/connect_screen.dart    scan list filtered by name "RoverC3" or the NUS service UUID
  ui/drive_screen.dart      joystick, speed chips 40/70/100, STOP button, status bar
```

Android permissions from the `flutter_blue_plus` README: `BLUETOOTH_SCAN` with
`neverForLocation`, `BLUETOOTH_CONNECT`; location for Android 11 and lower.

### Outline if B

- Copy Machi's `app/` here with a new application ID, and remove the features a rover doesn't
  use.
- Kotlin: a NUS link class built on `BuddyGatt`'s operation queue and back-off; a 50 ms send loop
  in the core while the drive page is open; bridge methods to connect, drive (l, r) and stop;
  the status line as an event.
- Flutter: a drive page with a joystick widget made from `GestureDetector` + `CustomPaint`.
