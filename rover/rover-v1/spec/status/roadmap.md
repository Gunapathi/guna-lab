# Roadmap

> **Owns:** the milestones, their order of work, the status of each item, later upgrades.
> **Item numbers are permanent:** code comments and the log cite them, so never renumber; add new
> items at the end.
> **Does not own:** the hardware checklists ([testing.md](testing.md)), what happened when
> ([log.md](log.md)).

---

**Status (2026-10-14):** the firmware for M0–M2 is built: it compiles for both boards and its
23 unit tests pass on the PC. Nothing has run on hardware yet. Next is the owner's hardware pass,
M0 → M1 → M2 ([testing.md](testing.md)); the app (M4) waits on the owner's decision
([app.md](../features/app.md#the-decision)).

## Milestones

Each milestone ends with a check you can see, and is finished on hardware before the next one
starts.

| # | Item | Milestone | Status |
|---|---|---|---|
| 1 | **Foundation:** the repo laid out like Machi (`firmware/`, `app/`, `spec/`), `CLAUDE.md`, `tool/guard.py`, the VS Code workspace | — | **Done 2026-10-14** |
| 2 | **Board alive:** the PlatformIO project (SuperMini + XIAO envs), `config.h`, the boot line with the reset reason, the 2 s heartbeat | M0 | Built 2026-10-14, on trial |
| 3 | **Motors from the serial monitor:** `motors.cpp` (LEDC, core 2.x and 3.x), the serial bench commands, the ramp | M1 | Built 2026-10-14, on trial |
| 4 | **Bluetooth with Bluefruit Connect:** RoverCore (protocol, mixer, drive) with Unity tests, the NUS server, the echo, Control Pad driving, the hello and status lines, the failsafes (disconnect, joystick timeout) | M2 | Built 2026-10-14 (tests pass on the PC), on trial |
| 5 | **On the chassis:** mount U3, U4 and the battery holder, wires away from the tracks; floor test; tune `MIN_DUTY`, `MAX_DUTY`, `RAMP_MS` ([tuning.md](../guides/tuning.md)) | M3 | Not started (hardware) |
| 6 | **The phone app:** find, joystick drive at 20 Hz, stop within 300 ms of closing the app or losing the link ([app.md](../features/app.md)) | M4 | Not started: the owner decides A or B first |

## Later upgrades (not now; the design just doesn't block them)

| # | Item | Notes |
|---|---|---|
| 7 | **Battery voltage** (3 × 10 kΩ + 10 kΩ divider → GPIO3) and a low-battery stop | [hardware.md](../core/hardware.md#later-wiring); the status line can gain a field (readers ignore unknown ones) |
| 8 | **Buzzer** on GPIO10 | [hardware.md](../core/hardware.md#later-wiring) |
| 9 | **ESP32-S3 Sense as a Wi-Fi camera** | Bluetooth is too slow for video |
| 10 | **Pairing:** only a paired phone may drive | An idea (agent, 2026-10-14), not decided. Machi's passkey pairing is the model; Bluefruit Connect would then have to pair too |
