# Hardware

> **Owns:** the modules, the pin map, the rev C test-harness wiring, power, the bench rules, the
> wiring for later items.
> **Does not own:** pin numbers in code (`firmware/include/config.h`), what the motors do
> ([drive.md](../features/drive.md)), fixes for symptoms
> ([troubleshooting.md](../guides/troubleshooting.md)).

---

Wiring follows the owner's **Tracked Rover Wiring – rev C (test harness)** page.

## Modules (rev C test harness)

| Ref | Module | Notes |
|---|---|---|
| BT1 | 2 × 18650 in series | 7.4 V nominal, 8.4 V full. Charged off the rover |
| SW1 | Power switch | On the pack's + |
| U3 | L298N | ENA, ENB and 5V-EN jumpers **on** |
| U4 | ESP32-C3 (SuperMini or Seeed XIAO ESP32C3) | Powered from the L298N's +5V. The owner's board is the **XIAO** (2026-10-14): [its pads](#xiao-esp32c3-pads) |
| M1, M2 | TT gear motors | Left, right |

Chassis: MakerWorld "Expandable Tracked Vehicle".

## Pin map

| Signal | GPIO | SuperMini label | XIAO label | L298N |
|---|---|---|---|---|
| Left motor A | 4 | 4 | D2 | IN1 |
| Left motor B | 5 | 5 | D3 | IN2 |
| Right motor A | 6 | 6 | D4 | IN3 |
| Right motor B | 7 | 7 | D5 | IN4 |
| 5 V in | — | 5V | 5V | +5V terminal (wire W5) |
| Ground | — | G | GND | GND terminal (wire W6) |

- Don't use GPIO **2, 8, 9**: boot strapping pins (9 is the BOOT button).
- Don't use GPIO **18, 19**: the USB port's D− / D+.
- Both boards use the same GPIOs, so one firmware serves both; only the PlatformIO board differs
  ([build.md](build.md)).

## XIAO ESP32C3 pads

The XIAO prints only the D labels, not GPIO numbers. Seen from the top, USB-C up:

```
                           USB-C
                       ┌───────────┐
 (not used) D0 GPIO2  ●│           │●  5V        ← W5 from L298N +5V
 (later)    D1 GPIO3  ●│           │●  GND       ← W6 to L298N GND
 IN1   ←    D2 GPIO4  ●│   XIAO    │●  3V3
 IN2   ←    D3 GPIO5  ●│  ESP32C3  │●  D10 GPIO10  (later: buzzer)
 IN3   ←    D4 GPIO6  ●│           │●  D9  GPIO9   (BOOT: don't use)
 IN4   ←    D5 GPIO7  ●│           │●  D8  GPIO8   (strapping: don't use)
            D6 GPIO21 ●│           │●  D7  GPIO20
                       └───────────┘
```

- **The 5V pad is joined to the USB-C port's 5V.** With W5 in and USB plugged, the L298N's
  regulator and the PC's USB port feed each other: that's why W5 comes out whenever USB is in
  ([bench rules](#bench-rules)).
- **The BAT+ / BAT− pads underneath are for one 3.7 V LiPo cell.** Never connect the 2-cell pack
  (up to 8.4 V) there or to the 5V pad directly; it only reaches the XIAO through the L298N's
  regulator and W5.
- **Plug in the U.FL antenna** that comes with it; without it Bluetooth reaches only a short way.
- The XIAO's 3.3 V outputs drive the L298N's inputs fine (it reads 2.3 V and up as high).

## Power

```
BT1 + ── SW1 ── L298N +12V terminal          L298N +5V ── W5 ── ESP32 5V
BT1 − ───────── L298N GND ─── W6 ─── ESP32 GND
L298N OUT1/OUT2 ── M1 (left)    L298N OUT3/OUT4 ── M2 (right)
```

- With 5V-EN on, the L298N's own regulator makes +5V from the pack; the ESP32 runs from it
  through W5.
- The L298N loses about 2 V as heat, so a full pack (8.4 V) gives the motors at most about
  6.4 V; `MAX_DUTY` 230 of 255 averages about 5.8 V, inside the TT motors' 3–6 V.
- **No battery protection:** switch off after testing, and recharge when the motors get weak
  (about 6.6 V across the pack, 3.3 V a cell).

## Bench rules

- **Pull wire W5 (L298N +5V → ESP32 5V) whenever the ESP32 is on USB.** Switch SW1 off while
  flashing.
- With W5 pulled, USB connected and SW1 on, the ESP32 runs from USB and the motors from the pack;
  W6 keeps the grounds shared, so serial commands drive the motors directly (M1).
- **Switch SW1 off before unplugging USB** on the bench: with the ESP32 dark and the pack on, the
  L298N's inputs float.
- Tracks lifted off the table for every first test.

## Later wiring

From the owner's rev A wiring page; roadmap items 7 and 8 ([roadmap.md](../status/roadmap.md)).

| Item | Wiring | Pin |
|---|---|---|
| Battery voltage | Divider: 3 × 10 kΩ in series from the pack's +, 10 kΩ to GND; the middle to the pin (8.4 V → 2.1 V) | GPIO3 (ADC; XIAO D1) |
| Buzzer | Buzzer on the pin | GPIO10 (XIAO D10) |
