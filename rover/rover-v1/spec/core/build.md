# Build, environment and definition of done

> **Owns:** the toolchain, pinned versions, the Arduino core choice, VS Code setup,
> build / upload / test commands, unit tests on the PC, known gotchas, the definition of done.
> **Does not own:** what the guard checks ([guards.md](guards.md)), what to build next
> ([roadmap.md](../status/roadmap.md)), wiring for the bench ([hardware.md](hardware.md#bench-rules)).

---

## Toolchain

| Thing | Version |
|---|---|
| Boards | **ESP32-C3 SuperMini** (env `supermini`, PlatformIO board `esp32-c3-devkitm-1`) or **Seeed XIAO ESP32C3** (env `xiao`, `seeed_xiao_esp32c3`). Both with native USB serial (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`) |
| PlatformIO Core | **6.2.0**, in `%USERPROFILE%\.platformio\penv`, where the VS Code PlatformIO IDE extension installs and reuses it. Usually not on PATH: use `$pio` below |
| Platform | `espressif32` **7.1.3** → `framework-arduinoespressif32` 4.20017 = **Arduino core 2.0.17** (ESP-IDF 4.4); RISC-V GCC 8.4; C++ `-std=gnu++11` |
| Libraries | **NimBLE-Arduino 2.5.1**; Unity 2.6.1 for the tests |
| Host C++ compiler | Only for the unit tests: [unit tests on the PC](#unit-tests-on-the-pc) |
| Flutter (M4) | 3.44.5 stable, plus the Dart and Flutter VS Code extensions |

**Sizes (2026-10-14, firmware 0.1.0):** SuperMini 502,214 bytes, XIAO 491,810 bytes of the
1,310,720-byte app partition (38 %); RAM 24 KB of 320 KB. The same sources also build on Arduino
core 3.3.1 (XIAO, 632,958 bytes).

## Versions

Pinned in `firmware/platformio.ini` to what was built and checked: `espressif32@7.1.3`,
`h2zero/NimBLE-Arduino@2.5.1`. To move one: change the pin, build both boards, run the unit
tests, do the M2 checklist on hardware ([testing.md](../status/testing.md)), and log it.

## Arduino core

PlatformIO's own `espressif32` platform still ships **Arduino core 2.x** (7.1.3, 2026-09), so
that is the build. Core 3.x renamed the LEDC calls; `motors.cpp` picks by
`ESP_ARDUINO_VERSION_MAJOR`:

| Core | Calls |
|---|---|
| 2.x | `ledcSetup(channel, PWM_FREQ_HZ, PWM_BITS)`, `ledcAttachPin(pin, channel)`, `ledcWrite(channel, duty)` (IN1–IN4 on channels 0–3) |
| 3.x | `ledcAttach(pin, PWM_FREQ_HZ, PWM_BITS)`, `ledcWrite(pin, duty)` |

To move to core 3.x, switch `platform` in `[esp32c3]` to a pinned release of the **pioarduino**
platform (`https://github.com/pioarduino/platform-espressif32/releases/…`), delete `.pio/`,
rebuild, and set the native env's `-std` to the one the new build uses (`pio run -v` shows it).

## VS Code

1. Install the **PlatformIO IDE** extension. It reuses the PlatformIO Core already in
   `%USERPROFILE%\.platformio`.
2. **File → Open Workspace from File → `rover-c3.code-workspace`.** PlatformIO only finds a
   project whose `platformio.ini` sits at the top of a workspace folder, so the workspace lists
   `firmware/` as a folder of its own next to the root. When the app exists, add `app/` the
   same way.
3. PlatformIO's status bar acts on `firmware/`: the env switcher (`env:supermini` / `env:xiao`),
   Build (✓), Upload (→), Serial Monitor (plug), Test (flask). In Arduino IDE terms: Tools →
   Board, Verify, Upload, Serial Monitor.
4. **PIO Home** (the house icon) isn't needed for this project: it's a dashboard for creating
   projects and browsing boards and libraries, and `platformio.ini` already holds all of that.
   The workspace keeps it from opening at startup (`platformio-ide.disablePIOHomeStartup`).

## Commands (PowerShell, from `firmware/`)

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"
& $pio run                          # build the XIAO (the default env: the owner's board)
& $pio run -e supermini             # build the SuperMini
& $pio run -t upload                # build + upload, XIAO (add -e supermini for a SuperMini)
& $pio device list                  # find the board's COM port
& $pio device monitor               # serial log at 115200 (Ctrl+C to quit)
& $pio run -t upload -t monitor     # upload, then watch
& $pio test -e native               # unit tests on the PC (needs gcc: below)
& $pio run -t clean                 # after odd build errors
```

- **Upload trouble:** hold **BOOT**, tap **RESET**, release BOOT, upload again. The bench rules
  apply while flashing: W5 pulled, SW1 off ([hardware.md](hardware.md#bench-rules)).
- **Odd builder errors:** delete `firmware\.pio\` and the cached `espressif32*` folders under
  `%USERPROFILE%\.platformio\platforms`, then rebuild.

## Unit tests on the PC

`pio test -e native` builds `lib/RoverCore` and `test/test_core` with the PC's `gcc` / `g++`,
which must be on PATH. Either:

- **MinGW-w64 (the plan's way):** `winget install BrechtSanders.WinLibs.POSIX.UCRT`, then open
  a new terminal.
- **PlatformIO's MinGW package** (GCC 5.1, no installer, nothing on the system PATH):
  ```powershell
  & $pio pkg install -g -t platformio/toolchain-gccmingw32@1.50100.0
  $env:PATH = "$env:USERPROFILE\.platformio\packages\toolchain-gccmingw32\bin;$env:PATH"   # this shell only
  ```
  `tool/guard.py` adds that folder to PATH by itself when it finds it.

The native env compiles with `-std=gnu++11`, the ESP32 build's standard, so RoverCore code that
passes the tests also builds for the board.

## Gotchas

- **First build:** about 9 minutes (PlatformIO downloads the platform, toolchain and framework
  into `%USERPROFILE%\.platformio`). After that a change builds in about 15 s.
- **Serial on the battery:** with no USB host, the C3's USB serial would block each print until a
  timeout and stall the control loop. `setup()` sets `Serial.setTxTimeoutMs(0)`, so output with
  no host is dropped instead ([serial.md](../features/serial.md)).
- **RESET re-enumerates USB** on native-USB C3 boards; PlatformIO's monitor reconnects. The boot
  line waits up to `SERIAL_WAIT_MS` (1.5 s) for it, with the motors already off.
- **`pio pkg list` crashes** with `UnicodeEncodeError` in a non-UTF-8 console: run
  `$env:PYTHONIOENCODING = "utf-8"` first.
- **NimBLE 2.x:**
  - Server callbacks take `NimBLEConnInfo&`; `onSubscribe` gets the subscribe value.
  - `advertiseOnDisconnect` is off by default.
  - `enableScanResponse(true)` must come before `setName`, or the name goes in the advert.
  - `NimBLEService::start()` is deprecated and unnecessary.
- **C library differences:** `snprintf` on a cut line returns −1 on old MinGW (and may not
  terminate). RoverCore's format helpers terminate and measure the string themselves.
- **Random resets once Bluetooth is up:** lower `BLE_TX_DBM` in `config.h`. Resets on motor
  starts are a different problem ([troubleshooting.md](../guides/troubleshooting.md)).

## Definition of done

1. `python tool/guard.py` passes: code map, doc links, protocol constants, RoverCore purity,
   trailers, both boards build, unit tests ([guards.md](guards.md)).
2. The milestone's checklist in [testing.md](../status/testing.md) passes **on real hardware**.
3. An entry for the session is added to [log.md](../status/log.md).
