# Rover C3 — Build Plan

Bluetooth-controlled tracked rover: ESP32-C3 + L298N + 2 × TT motors on the MakerWorld "Expandable Tracked Vehicle" chassis, driven from a phone.
Wiring follows the **Tracked Rover Wiring – rev C (test harness)** page. Build everything in VS Code.

---

## 1. Hardware (rev C test harness)

| Ref | Module | Notes |
|---|---|---|
| BT1 | 2 × 18650 in series | 7.4 V nominal, 8.4 V full. Charged off the rover |
| SW1 | Power switch | On pack + |
| U3 | L298N | ENA, ENB and 5V-EN jumpers **on** |
| U4 | ESP32-C3 (SuperMini or XIAO) | Powered from L298N +5V |
| M1, M2 | TT gear motors | Left, right |

### Pin map

| Signal | GPIO | SuperMini label | XIAO label | L298N |
|---|---|---|---|---|
| Left motor A | 4 | 4 | D2 | IN1 |
| Left motor B | 5 | 5 | D3 | IN2 |
| Right motor A | 6 | 6 | D4 | IN3 |
| Right motor B | 7 | 7 | D5 | IN4 |
| 5 V in | — | 5V | 5V | +5V terminal (wire W5) |
| Ground | — | G | GND | GND terminal (wire W6) |

Don't use GPIO 2, 8, 9 (boot strapping pins).

### Bench rules
- **Pull wire W5 (L298N +5V → ESP32 5V) whenever the ESP32 is on USB.** Switch SW1 off while flashing.
- Tracks lifted off the table for every first test.
- No battery protection: switch off after testing; recharge when motors get weak (~6.6 V across the pack).

---

## 2. Toolchain

1. VS Code extensions: **PlatformIO IDE** (firmware), **Flutter** + **Dart** (app, milestone M4).
2. Install the Adafruit **Bluefruit Connect** app on your phone (Play Store / App Store). It's the test remote until your own app exists.
3. Optional, for unit tests on the PC (M2): a host C++ compiler on PATH (`gcc`/`g++`). On Windows install MinGW-w64; on Linux it's already there.

### Framework choice
Use PlatformIO's standard `espressif32` platform (Arduino core **2.x**). The LEDC (PWM) calls in this plan are the core 2.x ones:

```cpp
ledcSetup(channel, PWM_FREQ_HZ, PWM_BITS);
ledcAttachPin(pin, channel);
ledcWrite(channel, duty);
```

If you later switch to the pioarduino platform (Arduino core 3.x), replace them with `ledcAttach(pin, freq, bits)` and `ledcWrite(pin, duty)`.

### `platformio.ini`

```ini
[platformio]
default_envs = supermini

[env]
platform = espressif32
framework = arduino
monitor_speed = 115200
lib_deps =
  h2zero/NimBLE-Arduino@^2.1.0

; ESP32-C3 SuperMini (generic C3 board + native USB serial)
[env:supermini]
board = esp32-c3-devkitm-1
build_flags =
  -D ARDUINO_USB_MODE=1
  -D ARDUINO_USB_CDC_ON_BOOT=1

; Seeed XIAO ESP32C3
[env:xiao]
board = seeed_xiao_esp32c3
build_flags =
  -D ARDUINO_USB_MODE=1
  -D ARDUINO_USB_CDC_ON_BOOT=1

; PC-side unit tests for the pure C++ logic in lib/
[env:native]
platform = native
lib_deps =
test_framework = unity
```

Upload trouble: hold **BOOT**, tap **RESET**, release BOOT, upload again. If PlatformIO throws odd builder errors, delete `.pio/` and the cached `espressif32*` folders under `~/.platformio/platforms`, then rebuild.

---

## 3. Firmware architecture

```
rover-c3/
├─ platformio.ini
├─ include/
│  └─ config.h              # every pin, limit and timing constant
├─ lib/
│  └─ RoverCore/            # pure C++, no Arduino headers → testable on PC
│     ├─ protocol.h/.cpp    # checksum, stream packet parser
│     └─ mixer.h/.cpp       # control-pad state → left/right %, slew limiter
├─ src/
│  ├─ main.cpp              # setup/loop, wires modules together
│  ├─ motors.h/.cpp         # L298N via LEDC PWM
│  ├─ ble_link.h/.cpp       # NimBLE Nordic UART Service (NUS)
│  └─ serial_cmd.h/.cpp     # bench commands over USB serial (M1)
└─ test/
   └─ test_core/test_main.cpp   # Unity tests for lib/RoverCore
```

### Data flow

```
Phone ──BLE write──► ble_link (NimBLE task)
                       │ copy bytes into a ring buffer (no motor calls here)
                       ▼
loop(): drain buffer → PacketParser → Packet
          ├─ 'B' button  → PadState → mixer → target L/R %
          └─ 'J' joystick → target L/R %
        every 10 ms: slew limiter → motors.set(L, R)
        failsafe: disconnect / joystick timeout → target 0
```

Rule: **BLE callbacks never touch the motors.** They only push bytes (and a connected/disconnected flag) for `loop()` to handle.

### `include/config.h`

```cpp
#pragma once
// Pins (L298N inputs)
constexpr int PIN_IN1 = 4;   // left A
constexpr int PIN_IN2 = 5;   // left B
constexpr int PIN_IN3 = 6;   // right A
constexpr int PIN_IN4 = 7;   // right B

// PWM — the L298N is slow, keep it around 1 kHz
constexpr int PWM_FREQ_HZ = 1000;
constexpr int PWM_BITS    = 8;      // duty 0..255

// Motor limits (tune in M5)
constexpr int MAX_DUTY = 230;       // ~5.8 V average at a full pack; TT motors are 3–6 V parts
constexpr int MIN_DUTY = 90;        // below this TT motors hum but don't turn
constexpr bool INVERT_LEFT  = false;
constexpr bool INVERT_RIGHT = false;

// Behaviour
constexpr int SPEED_LEVELS_PCT[3] = {40, 70, 100};
constexpr int RAMP_MS        = 300;   // 0 → full speed time
constexpr int CONTROL_TICK_MS = 10;
constexpr int JOY_TIMEOUT_MS = 300;   // stop if joystick packets stop

// BLE
constexpr const char* DEVICE_NAME = "RoverC3";
```

### Module contracts

| Module | API | Behaviour |
|---|---|---|
| `motors` | `void motorsBegin(); void motorsSet(int leftPct, int rightPct);` | Inputs −100..100. Sign picks the pin: `>0` → A = PWM, B = 0; `<0` → A = 0, B = PWM; `0` → both 0 (coast). Non-zero maps to `MIN_DUTY..MAX_DUTY`. Applies `INVERT_*`. `motorsBegin()` drives all four pins LOW **first thing** in `setup()`. |
| `protocol` | `uint8_t checksum(const uint8_t*, size_t); bool PacketParser::feed(uint8_t b, Packet& out);` | Byte-by-byte state machine: wait for `!`, read type, read payload length for that type, verify checksum, emit. Bad checksum or unknown type → reset and resync on next `!`. |
| `mixer` | `void padToTracks(const PadState&, int& l, int& r); int slew(int current, int target, int maxStep);` | See tables in §5. `slew` moves at most `maxStep` per tick (100 % / (RAMP_MS / CONTROL_TICK_MS) ≈ 3 % per tick). |
| `ble_link` | `void bleBegin(); bool bleConnected(); size_t bleRead(uint8_t* buf, size_t max); void bleSendStatus(const char*);` | NUS server, advertises `DEVICE_NAME` and the NUS service UUID, restarts advertising on disconnect (NimBLE 2.x doesn't do it by default: call `advertiseOnDisconnect(true)` on the server). |
| `serial_cmd` | `void serialPoll(Target&);` | Single-letter bench commands (M1). |

NimBLE-Arduino 2.x notes: connection callbacks now take `NimBLEConnInfo&` (e.g. `onConnect(NimBLEServer*, NimBLEConnInfo&)`); name and scan response must be set explicitly on the advertising object (`setName`, `enableScanResponse(true)`). Check the library's bundled examples for the exact `onWrite`/`onDisconnect` signatures of the version you install.

---

## 4. Bluetooth protocol

Transport: **Nordic UART Service (NUS)**, which Bluefruit Connect understands.

| Item | UUID | Direction |
|---|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | — |
| RX characteristic | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` | phone → rover (write, write without response) |
| TX characteristic | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` | rover → phone (notify) |

### Packets (phone → rover)

All packets start with `!`, then a type byte, then payload, then **checksum = bitwise NOT of the byte sum of everything before it, low 8 bits**.

```cpp
uint8_t checksum(const uint8_t* p, size_t n) {
  uint8_t s = 0;
  for (size_t i = 0; i < n; i++) s += p[i];
  return (uint8_t)~s;
}
```

| Type | Bytes | Payload | Source |
|---|---|---|---|
| Button | 5 | `!` `B` `<button '1'..'8'>` `<'1' pressed / '0' released>` `<crc>` | Bluefruit Control Pad |
| Joystick | 5 | `!` `J` `<left int8 −100..100>` `<right int8 −100..100>` `<crc>` | Your Flutter app (M4) |

Bluefruit button codes: `1`–`4` = numbered buttons, `5` = Up, `6` = Down, `7` = Left, `8` = Right.

Worked examples (use these as unit-test vectors):

| Meaning | Bytes (hex) |
|---|---|
| Up pressed `!B51` | `21 42 35 31 36` |
| Up released `!B50` | `21 42 35 30 37` |
| Joystick L = +60, R = +60 | `21 4A 3C 3C 1C` |
| Joystick L = −60, R = +40 (turn left) | `21 4A C4 28 A8` |
| Joystick stop L = 0, R = 0 | `21 4A 00 00 94` |

### Status (rover → phone)
Plain text line on TX, at most once per second and on every change, e.g. `OK spd=2 L=70 R=70\n`. Bluefruit's UART screen shows it; your app shows it in a status bar.

---

## 5. Drive logic

### Control Pad (Bluefruit)
Keep a `PadState { up, down, left, right, level }` updated from press/release packets. `s = SPEED_LEVELS_PCT[level]`.

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

Numbered buttons (on press): `1` = speed 40 %, `2` = 70 %, `3` = 100 %, `4` = stop (clear all held directions).

### Joystick (own app)
Packet values are the final track speeds; the app does the mixing (§7).

### Failsafes
| Event | Action |
|---|---|
| BLE disconnect | Target 0 **immediately** (skip the ramp), clear PadState |
| Joystick mode and no `J` packet for `JOY_TIMEOUT_MS` | Target 0 |
| Button mode | No timeout: the Control Pad only sends on press/release, so a timeout would stop a held button. Release + disconnect cover it |
| Boot | IN1–IN4 LOW before anything else |

---

## 6. Milestones

Each milestone ends with a check you can see. Don't start the next one until it passes.

### M0 — Board alive
- [ ] Create the PlatformIO project (board per §2), add `config.h`.
- [ ] `setup()` prints `RoverC3 boot` and the reset reason; `loop()` prints uptime every 2 s.
- **Done when:** Serial Monitor shows the lines after pressing RESET.

### M1 — Motors from the serial monitor (no Bluetooth)
- [ ] `motors.cpp`: LEDC on 4 channels, `motorsSet()` per §3.
- [ ] `serial_cmd.cpp`: `f` forward, `b` back, `l` spin left, `r` spin right, `s` stop, `1`/`2`/`3` speed level.
- [ ] Slew limiter in `loop()` every `CONTROL_TICK_MS`.
- [ ] Bench setup: **W5 pulled, USB connected, SW1 on.** The ESP32 runs from USB and the motors from the pack; W6 keeps the grounds shared, so serial commands drive the motors directly.
- **Done when:** each command moves both tracks the right way. Fix wrong directions with `INVERT_LEFT/RIGHT` (or swap that motor's two wires).

### M2 — Bluetooth with Bluefruit Connect
- [ ] `lib/RoverCore/protocol` + `mixer` with Unity tests in `test/test_core/` using the vectors in §4 and the table in §5 (`pio test -e native`).
- [ ] `ble_link.cpp`: NUS server, ring buffer from `onWrite`, connect/disconnect flags, advertising restart.
- [ ] In Bluefruit Connect → UART: anything you type is echoed back on TX (sanity check).
- [ ] Controller → Control Pad drives per §5; status line on TX.
- **Done when:** `pio test -e native` passes; you can drive with the pad; turning off phone Bluetooth stops the rover within ~100 ms.

### M3 — On the chassis
- [ ] Mount U3, U4 and the battery holder; zip-tie wires away from the tracks.
- [ ] Floor test. Tune `MIN_DUTY` (lowest that still moves on the floor), `MAX_DUTY`, `RAMP_MS`.
- **Done when:** it drives, turns in place and doesn't reset the ESP32 on hard starts or reversals.

### M4 — Your Flutter app
See §7.
- **Done when:** the app finds `RoverC3`, drives it with the joystick, and the rover stops within 300 ms of closing the app or losing the connection.

### Later
- Battery voltage divider (3 × 10 kΩ + 10 kΩ → GPIO3) and low-battery stop; buzzer on GPIO10 (rev A wiring page).
- ESP32-S3 Sense as a Wi-Fi camera (BLE is too slow for video).

---

## 7. Flutter app plan

### Packages
- `flutter_blue_plus` — BLE central. BLE only (no classic Bluetooth). Free for individuals and orgs under 15 employees.
- `flutter_joystick` (0.2.x) — on-screen stick; listener gives x/y in −1..+1.

Add the Android permissions from the `flutter_blue_plus` README (`BLUETOOTH_SCAN` with `neverForLocation`, `BLUETOOTH_CONNECT`; location permission for Android 11 and lower).

### Structure

```
rover_app/lib/
├─ main.dart
├─ protocol/packets.dart     # checksum(), joystickPacket(l, r) → Uint8List
├─ drive/mixer.dart          # (x, y) → (left, right) in −100..100
├─ ble/rover_link.dart       # scan, connect, find RX/TX by UUID, send(), status stream
└─ ui/
   ├─ connect_screen.dart    # scan list filtered by name "RoverC3" or the NUS service UUID
   └─ drive_screen.dart      # joystick, speed chips 40/70/100, STOP button, status bar
```

### Behaviour
- **Mixer:** `y = -stickY` (screen up is negative; verify on your phone), deadzone 0.08, `left = clamp(y + x)`, `right = clamp(y − x)`, scale by speed chip, round to int −100..100.
- **Send loop:** `Timer.periodic(50 ms)` sends the latest `J` packet with **write without response**. This 20 Hz stream is what keeps the 300 ms firmware failsafe fed.
- **Release:** joystick released → send `J 0 0` at once.
- **Lifecycle:** on `AppLifecycleState.paused` or `detached` → send `J 0 0`, then disconnect.
- **Status:** subscribe to TX notify, show the latest line.
- Unit-test `packets.dart` with the same hex vectors as the firmware (§4).

---

## 8. Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| ESP32 resets when motors start or reverse | Voltage dip | Longer `RAMP_MS`; check W6 ground; lower `MAX_DUTY` |
| Motors twitch at power-on | IN pins floating at boot | Set IN1–IN4 LOW first in `setup()` |
| One track never moves | ENA/ENB jumper missing, or OUT wire loose | Refit jumper, check screw terminals |
| Track runs backwards | Motor wires reversed | `INVERT_LEFT/RIGHT` or swap that motor's wires |
| Motor hums at low speed | Duty too low | Raise `MIN_DUTY` |
| Phone can't find `RoverC3` | Name not in advert / location off (old Android) | `setName` + `enableScanResponse(true)`; turn on location |
| Second connection fails after a disconnect | Advertising not restarted | `advertiseOnDisconnect(true)` |
| Upload fails | Board not in download mode | Hold BOOT, tap RESET, upload |
| L298N gets hot | Normal; it loses ~2 V as heat | Keep `MAX_DUTY` ≤ 230; give it airflow |
