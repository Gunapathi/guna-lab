# Testing: what's on trial

> **Owns:** the hardware checklists for each milestone, what is waiting to be checked, and the
> tuned values.
> **Does not own:** the rules being checked (each line links to its owner), the build commands
> ([build.md](../core/build.md)), the phone steps ([bluefruit-test.md](../guides/bluefruit-test.md)).

---

**On trial (2026-10-14):** M0, M1 and M2, all built in one session before any hardware check.
Go top to bottom in one sitting. A box that fails: note it under its line (date, what happened)
and keep going; the notes are the next session's work list. Once a milestone is all ticked, mark
it Done in [roadmap.md](roadmap.md) and add a line to [log.md](log.md).

**Already checked on the PC (2026-10-14):** both boards build; `pio test -e native` passes 23
tests (protocol vectors, the pad table, the ramp, both failsafes).

## M0: board alive

Board on USB, W5 pulled, SW1 off. `pio run -t upload -t monitor`.

- [x] Upload works (if not: BOOT + RESET, [build.md](../core/build.md#commands-powershell-from-firmware)).
- [x] After RESET: `RoverC3 boot  fw 0.1.0  protocol v1  reset: …`, then `Bluetooth advertising…`.
- [x] A heartbeat line every 2 s: `up …s  link off  spd 1  target 0/0  out 0/0`.
- [x] Both boards, if you have both: `-e supermini` and `-e xiao`.

## M1: motors from the serial monitor

Tracks lifted. **W5 pulled, USB connected, SW1 on** ([hardware.md](../core/hardware.md#bench-rules)).

- [ ] Nothing moves at power-on or after RESET.
- [ ] `f`: both tracks forward, ramping up. `s`: both stop.
- [ ] `b`: both back. `l`: spin left (left back, right forward). `r`: spin right.
- [ ] `3` while `f` runs: faster at once (ramped). `1`: slower.
- [ ] `f` straight to `b`: the tracks slow through zero and reverse; no reset.
- [ ] A wrong direction is fixed with `INVERT_LEFT` / `INVERT_RIGHT`, or by swapping that motor's
      wires ([tuning.md](../guides/tuning.md#1-directions-tracks-lifted)). Note which.
- [ ] `h` prints the commands; `x` says it's not a command.

## M2: Bluetooth with Bluefruit Connect

Steps in [bluefruit-test.md](../guides/bluefruit-test.md). Tracks lifted. On USB (W5 pulled) to
watch serial, then once on the battery alone (W5 in, USB out).

- [ ] Bluefruit Connect lists **RoverC3**; it connects; serial `Bluetooth connected`.
- [ ] UART: `HELLO RoverC3 fw=0.1.0 pv=1`, then `OK spd=1 L=0 R=0` every second.
- [ ] UART: typed text comes back.
- [ ] Control Pad: every row of the table in [bluefruit-test.md](../guides/bluefruit-test.md#bluefruit-connect-control-pad) behaves as listed.
- [ ] Turning off the phone's Bluetooth while driving stops the rover within about 100 ms.
- [ ] Reconnecting works without a restart (advertising came back).
- [ ] nRF Connect: `214A3C3C1C` drives about 0.3 s, then serial `Failsafe: no joystick packet in time, stopped`.
- [ ] nRF Connect: a bad checksum (`214A3C3C1D`) does nothing.
- [ ] On the battery alone (no USB): drives the same, and doesn't stall (printing doesn't block).

## M3: on the chassis

- [ ] Mounted: U3, U4, the battery holder; wires zip-tied away from the tracks.
- [ ] Drives forward and back, turns in place, curves with Up + Left / Right.
- [ ] Hard starts and full-speed reversals don't reset the ESP32 (no `BROWNOUT` on the boot line).
- [ ] Tuned ([tuning.md](../guides/tuning.md)); record below.

**Tuned values:** (date, board, pack voltage)

| Value | Plan default | Tuned |
|---|---|---|
| `MIN_DUTY` | 90 | |
| `MAX_DUTY` | 230 | |
| `RAMP_MS` | 300 | |
| `INVERT_LEFT` / `INVERT_RIGHT` | false / false | |

## M4: the phone app

Written when the app is built ([app.md](../features/app.md)).
