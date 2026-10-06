# Digi Eyes

Animated face and mini smart display on a **Seeed Studio XIAO ESP32C3** with a 0.96" SSD1306 OLED,
controlled from the phone's browser, with phone notifications over Bluetooth (Chronos app).

Each version is a complete sketch. Open `<version>/<version>.ino` in the Arduino IDE (Arduino needs the
folder and the `.ino` to share a name). Full setup, wiring and API docs are in [v2/README.md](v2/README.md).

| Version | Date | What's new |
|---|---|---|
| [v1](v1/) | 2026-10-03 | Cosmo-style eyes (9 faces), clock, notifications, calls, battery and info screens, Chronos Bluetooth. Joins home WiFi, falls back to a `DigiEyes-Setup` hotspot. |
| [v2](v2/) | 2026-10-05 | Runs its own `DigiEyes` hotspot by default (`HOTSPOT_ONLY`), QR codes to join it and open the control page, captive portal, phone clock sync. |
| [v2.1](v2.1/) | 2026-10-06 | New cute face (eyes, blush, mouth, props) with 9 animations: normal, happy, snoring, angry, love, alone, confused, hangover, surprised. Picked faces stay up for `FACE_SCREEN_MS`. |

## Build

- Board: esp32 → **XIAO_ESP32C3**, Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**
- Libraries: Adafruit GFX, Adafruit SSD1306, ChronosESP32 (install all: NimBLE-Arduino, ESP32Time)

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3:PartitionScheme=huge_app digi-eyes/v2.1
```

## Secrets

v1 reads your real home WiFi from `v1/secrets.h`, which is git-ignored. Without it, `config.h` uses the
`YOUR_WIFI_NAME` / `YOUR_WIFI_PASSWORD` placeholders. To use home WiFi with v1, rename (or copy)
`v1/secrets-dummy.h` to `v1/secrets.h` and put your WiFi name and password in it:

```c
#pragma once
#define HOME_WIFI_SSID "your-wifi"
#define HOME_WIFI_PASS "your-password"
```

## Publishing

`spec.md` files are private notes. To share a tested version, run
`tools/publish.ps1 -Project digi-eyes -Version v2.1`. It copies the version to the public
`guna-lab` repo without `spec.md` or `secrets.h`, and stops if it finds real credentials.
