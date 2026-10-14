# Drive

> **Owns:** who drives (pad, joystick, bench), the Control Pad table, speed levels, the joystick,
> the ramp, motor output, the failsafes.
> **Does not own:** packet bytes ([protocol.md](../core/protocol.md)), the serial command letters
> ([serial.md](serial.md)), tuned values ([tuning.md](../guides/tuning.md)), pins
> ([hardware.md](../core/hardware.md)).

---

## Who drives

Three sources set the target track speeds. The last one used wins.

| Source | Sets | Timeout |
|---|---|---|
| Control Pad (B packets) | Held directions + the speed level → the [table](#control-pad) | None |
| Joystick (J packets, the own app) | The two track speeds directly | `JOY_TIMEOUT_MS` (300 ms) |
| Serial bench ([serial.md](serial.md)) | One direction, held until `s`; the speed level | None |

- A J packet switches to joystick mode and forgets any held pad direction, so going back to the
  pad never resumes a stale one.
- Any B packet or bench command switches back to pad mode.
- The pad and the bench share one speed level. It starts at the slowest, 40 % (`START_LEVEL`).

## Control Pad

`s` = the speed of the current level. Up and Down cancel each other, and so do Left and Right,
so Up + Down + Left spins left.

| Held | Left % | Right % |
|---|---|---|
| nothing, or Up + Down | 0 | 0 |
| Up | +s | +s |
| Down | −s | −s |
| Left | −s | +s (spin) |
| Right | +s | −s (spin) |
| Up + Left | +s/2 | +s |
| Up + Right | +s | +s/2 |
| Down + Left | −s/2 | −s |
| Down + Right | −s | −s/2 |

Numbered buttons, on press: **1** = 40 %, **2** = 70 %, **3** = 100 % (`SPEED_LEVELS_PCT`),
**4** = stop (forgets every held direction). A new level applies at once to whatever is held.

## Joystick

The values are the final track speeds: the app does the mixing
([app.md](app.md#what-any-app-must-do)). Out-of-range values are clamped to −100..100. The app
sends at 20 Hz; the rover stops if the stream stops ([failsafes](#failsafes)).

## Ramp

Every target change is ramped. Each control tick (`CONTROL_TICK_MS`, 10 ms) a track moves at most
`SLEW_STEP_PCT` toward its target. That is 4 %, rounded up from `RAMP_MS`, so 0 → 100 % takes
250 ms, within `RAMP_MS` (300). A reversal ramps through 0. Only the failsafes skip the ramp.

## Motor output

Per track, the % after the ramp becomes PWM on one of its two L298N inputs (`motorDuty`):

- **0** → both pins 0: the motor **coasts**. Braking (both pins high) is never used.
- **1..100** → duty `MIN_DUTY`..`MAX_DUTY` (90..230 of 255), linear. TT motors hum but don't
  turn below `MIN_DUTY`; `MAX_DUTY` keeps them near 5.8 V on a full pack
  ([hardware.md](../core/hardware.md#power)).
- **The sign picks the pin:** > 0 → A = PWM, B = 0; < 0 → A = 0, B = PWM. The pin going quiet is
  written first, so A and B are never driven together.
- `INVERT_LEFT` / `INVERT_RIGHT` flip a track whose motor is wired backwards.
- PWM: 1 kHz (the L298N is slow), 8 bits.

## Failsafes

| Event | Action |
|---|---|
| Power-on | IN1–IN4 driven LOW first thing in `setup()`, before serial and Bluetooth |
| Bluetooth disconnect | Stop **at once** (targets and outputs 0, no ramp), forget held directions, drop a half-read packet |
| Joystick mode, no J packet for more than `JOY_TIMEOUT_MS` | Stop **at once**. The next J packet drives again, ramped |
| Pad or bench mode | No timeout: the pad only sends on press and release, so a timeout would stop a held button. Release, button 4, `s` and a disconnect cover it |

**Why at once:** stopping coasts (both pins low) and draws no current, so the ramp's job (soft
starts and reversals) doesn't apply. It also keeps M4's "stops within 300 ms of losing the app"
possible; a 300 ms ramp after a 300 ms timeout would not.

**Timing:** a joystick stop comes 300–310 ms after the last J packet (the timeout, then the next
tick). A disconnect stops on the next `loop()` pass after NimBLE reports it.
