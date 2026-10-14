# Product: Rover C3

> **Owns:** what the rover is, who it's for, the confirmed decisions, the open ones, a short user
> guide. Owns no technical rule.

---

**Rover C3** is a small tracked rover driven from a phone over Bluetooth. An ESP32-C3
(SuperMini or Seeed XIAO) drives two TT gear motors through an L298N, on the MakerWorld
"Expandable Tracked Vehicle" chassis, powered by two 18650 cells.

Personal project: one owner, one rover, one phone at a time. Until the rover's own app exists
(M4), the remote is Adafruit's **Bluefruit Connect** app and its Control Pad.

## Confirmed decisions (owner's plan, 2026-10-14)

| Topic | Decision |
|---|---|
| Board | ESP32-C3 SuperMini or Seeed XIAO ESP32C3: one firmware, the same GPIOs, a build env for each. The owner has the **XIAO** (2026-10-14), so `xiao` is the default env |
| Motor driver | L298N with the ENA, ENB and 5V-EN jumpers on; PWM on the IN pins |
| Power | 2 × 18650 in series (7.4 V nominal, 8.4 V full), charged off the rover; no battery protection yet |
| Link | Bluetooth LE, Nordic UART Service, so Bluefruit Connect works as the first remote |
| Packets | Bluefruit's `!` packets: its Control Pad buttons, plus a `J` joystick packet for the own app |
| Toolchain | VS Code + PlatformIO (Arduino core 2.x); unit tests on the PC |
| Safety | Motors off at power-on, on disconnect, and when joystick packets stop |
| Repo | Firmware and phone app in one folder (`firmware/`, `app/`), docs in `spec/`, laid out like the owner's Machi project |

## Open decisions

| Topic | Options | Where |
|---|---|---|
| The phone app (M4) | A new Flutter app (`flutter_blue_plus` + a joystick), or start from Machi's app (Flutter UI + Kotlin core) | [app.md](features/app.md#the-decision) |

## User guide (short)

1. Switch on (SW1). Nothing moves; the rover advertises as **RoverC3**.
2. Bluefruit Connect → **RoverC3** → Controller → **Control Pad**.
3. Hold an arrow to drive (Up + Left curves left). **1** / **2** / **3** set the speed to 40 / 70
   / 100 %, **4** stops.
4. Close the app or switch off the phone's Bluetooth: the rover stops at once.
5. Switch off after driving. Recharge when it gets weak (about 6.6 V across the pack).
