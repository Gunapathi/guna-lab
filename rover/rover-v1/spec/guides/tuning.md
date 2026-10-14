# Tuning on the floor (M3)

> **Owns:** the procedure for setting the motor directions, `MIN_DUTY`, `MAX_DUTY` and `RAMP_MS`
> on the real chassis.
> **Does not own:** what those values do ([drive.md](../features/drive.md#motor-output)), where
> the results are recorded ([testing.md](../status/testing.md#m3-on-the-chassis)).

---

Each value lives in `firmware/include/config.h`. Change one, upload (`pio run -t upload`), test,
repeat. Use the serial bench commands ([serial.md](../features/serial.md)) while on USB (W5
pulled), or the Control Pad on the battery.

## 1. Directions (tracks lifted)

1. `f`: both tracks must run forward. `l`: the rover would spin left (left track back, right
   forward).
2. A track running backwards: set its `INVERT_LEFT` / `INVERT_RIGHT` to `true`, or swap that
   motor's two wires on the L298N.
3. Left and right swapped (`l` spins right): swap the two motors' plugs, not the code.

## 2. `MIN_DUTY` (on the floor)

The lowest duty that still moves the rover from standstill. Below it the motors hum.

1. Temporarily set `SPEED_LEVELS_PCT` to `{1, 70, 100}`: speed 1 then runs at exactly
   `MIN_DUTY`.
2. Start with `MIN_DUTY` 60. Speed `1`, `f`: does the rover move off reliably, on the floor it
   will drive on?
3. Raise `MIN_DUTY` by 10 until it always moves off, then add 10 as margin.
4. Put `SPEED_LEVELS_PCT` back to `{40, 70, 100}`.

## 3. `MAX_DUTY`

Keep it at 230 or lower (about 5.8 V average on a full pack, inside the TT motors' 3–6 V; the
L298N runs hot above that). Lower it if full speed is too fast to control, or if a reversal at
full speed resets the ESP32.

## 4. `RAMP_MS`

At speed `3`, drive forward, then reverse at once (`f`, then `b`), several times.

- The boot line shows `reset: BROWNOUT`, or serial restarts: the ESP32 browned out. Raise
  `RAMP_MS` (400, 500), check the W6 ground, or lower `MAX_DUTY`.
- No resets but it feels sluggish: lower `RAMP_MS` (200).

## 5. Record

Write the final values and the date in [testing.md](../status/testing.md#m3-on-the-chassis), and
a line in [log.md](../status/log.md). If the rover curves at equal speeds, note how much: a trim
is not built yet.
