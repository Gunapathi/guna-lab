# USB serial

> **Owns:** what the rover prints over USB serial (boot line, heartbeat, events), and the bench
> commands.
> **Does not own:** what a bench command does to the tracks ([drive.md](drive.md)), the bench
> wiring ([hardware.md](../core/hardware.md#bench-rules)), how to open the monitor
> ([build.md](../core/build.md#commands-powershell-from-firmware)).

---

115200 baud (the C3's USB serial ignores it, but the monitor needs a number).

## Boot line

```
RoverC3 boot  fw 0.1.0  protocol v1  reset: power-on
Bluetooth advertising. Serial: h for the bench commands.
```

The reset reason is `power-on`, `reset pin`, `software`, `crash`, `watchdog`, `deep sleep`,
`BROWNOUT` or `other`. A **BROWNOUT** after a hard start or reversal means the pack's voltage
dipped ([troubleshooting.md](../guides/troubleshooting.md)).

## Heartbeat

Every 2 s (`HEARTBEAT_MS`):

```
up 12s  link off  spd 1  target 0/0  out 0/0
```

`target` is what was asked for (left/right %), `out` what the motors get after the ramp.

## Events

`Bluetooth connected`, `Bluetooth disconnected: stopped`,
`Failsafe: no joystick packet in time, stopped`.

## Bench commands

One key each; PlatformIO's monitor sends a key as you type it, without Enter. A direction holds
until `s` or another direction, like a held pad button.

| Key | Does |
|---|---|
| `f` | Forward at the current speed |
| `b` | Back |
| `l` | Spin left |
| `r` | Spin right |
| `s` | Stop |
| `1` / `2` / `3` | Speed 40 / 70 / 100 %, keeping the direction |
| `h` or `?` | This list |

The rover answers `> f  spd 1  target L 40 R 40`, or `? 'x' is not a command (h for help)`.
Enter, carriage return and space are ignored.

## Printing never blocks

On the battery there's no USB host. `setup()` sets the serial TX timeout to 0, so anything printed
then is dropped instead of stalling the control loop
([build.md](../core/build.md#gotchas)).
