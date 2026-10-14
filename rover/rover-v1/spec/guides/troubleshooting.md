# Troubleshooting

> **Owns:** symptom → likely cause → fix, for the rover and its build.
> **Does not own:** the rules behind the fixes (each links to its home).

---

| Symptom | Likely cause | Fix |
|---|---|---|
| ESP32 resets when motors start or reverse; the boot line says `reset: BROWNOUT` | Voltage dip | Longer `RAMP_MS`; check the W6 ground; lower `MAX_DUTY` ([tuning.md](tuning.md#4-ramp_ms)) |
| Motors twitch at power-on | IN pins floating at boot | `motorsBegin()` must stay first in `setup()` ([drive.md](../features/drive.md#failsafes)) |
| Motors twitch when USB is unplugged on the bench | The ESP32 went dark with the pack on; the L298N's inputs float | Switch SW1 off before unplugging USB ([hardware.md](../core/hardware.md#bench-rules)) |
| One track never moves | ENA/ENB jumper missing, or an OUT wire loose | Refit the jumper, check the screw terminals |
| A track runs backwards | Motor wires reversed | `INVERT_LEFT` / `INVERT_RIGHT`, or swap that motor's wires ([tuning.md](tuning.md#1-directions-tracks-lifted)) |
| A motor hums at low speed but doesn't turn | Duty too low | Raise `MIN_DUTY` ([tuning.md](tuning.md#2-min_duty-on-the-floor)) |
| The rover stops by itself every 300 ms | Joystick mode without a steady 20 Hz stream (e.g. single writes from nRF Connect) | Expected: that's the failsafe. The app must send every 50 ms ([app.md](../features/app.md#what-any-app-must-do)) |
| Phone can't find `RoverC3` | Another phone is connected; or location is off (Android 11 and lower) | Disconnect the other phone; turn on location. The name is in the scan response, so use an active scan |
| A second connection fails after a disconnect | Advertising not restarted | `advertiseOnDisconnect(true)` in `bleBegin()` ([link.md](../features/link.md#advertising)) |
| Status lines arrive in pieces | The link's MTU is the default 23 | Expected: join notifications and split on `\n` ([protocol.md](../core/protocol.md#rover--phone-tx)) |
| Nothing on the serial monitor after RESET | The C3's USB port re-enumerated after the monitor opened | Wait: the boot line waits 1.5 s for the monitor; the heartbeat follows every 2 s |
| Upload fails | Board not in download mode | Hold BOOT, tap RESET, release BOOT, upload ([build.md](../core/build.md#commands-powershell-from-firmware)) |
| Odd PlatformIO builder errors | A stale build cache | Delete `firmware\.pio\` and the cached `espressif32*` platforms, rebuild |
| `pio test -e native`: `'gcc' is not recognized` | No host compiler on PATH | [build.md](../core/build.md#unit-tests-on-the-pc) |
| L298N gets hot | Normal: it loses about 2 V as heat | Keep `MAX_DUTY` ≤ 230; give it airflow |
