#pragma once
// =====================================================================
//  Rover C3 firmware: every pin, limit, timing and ID lives here.
//  Change values here, never inline in the code. Spec: spec/index.md
// =====================================================================

// ---------- Identity ----------
constexpr const char* FW_VERSION = "0.1.0";
constexpr int PROTOCOL_VERSION = 1;   // spec/core/protocol.md: change the doc first, then bump

// ---------- Pins: L298N inputs (spec/core/hardware.md) ----------
// Same GPIOs on both boards. Never use 2, 8 or 9: boot strapping pins.
constexpr int PIN_IN1 = 4;   // left motor A   (SuperMini 4, XIAO D2)
constexpr int PIN_IN2 = 5;   // left motor B   (SuperMini 5, XIAO D3)
constexpr int PIN_IN3 = 6;   // right motor A  (SuperMini 6, XIAO D4)
constexpr int PIN_IN4 = 7;   // right motor B  (SuperMini 7, XIAO D5)

// ---------- PWM ----------
constexpr int PWM_FREQ_HZ = 1000;   // the L298N is slow: keep it around 1 kHz
constexpr int PWM_BITS    = 8;      // duty 0..255

// ---------- Motor limits (tune in M3: spec/guides/tuning.md) ----------
constexpr int MAX_DUTY = 230;       // ~5.8 V average at a full pack; TT motors are 3-6 V parts
constexpr int MIN_DUTY = 90;        // below this TT motors hum but don't turn
constexpr bool INVERT_LEFT  = false;
constexpr bool INVERT_RIGHT = false;

// ---------- Behaviour (spec/features/drive.md) ----------
constexpr int SPEED_LEVELS_PCT[3] = {40, 70, 100};   // buttons 1, 2, 3
constexpr int SPEED_LEVELS    = 3;
constexpr int START_LEVEL     = 0;    // index into SPEED_LEVELS_PCT at power-on: the slowest
constexpr int RAMP_MS         = 300;  // 0 -> full speed takes at most this long
constexpr int CONTROL_TICK_MS = 10;
constexpr int SLEW_STEP_PCT   = (100 * CONTROL_TICK_MS + RAMP_MS - 1) / RAMP_MS;   // % per tick, rounded up
constexpr int JOY_TIMEOUT_MS  = 300;  // joystick mode: stop if J packets stop

// ---------- Bluetooth (spec/core/protocol.md) ----------
constexpr const char* DEVICE_NAME      = "RoverC3";
constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_RX_UUID      = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";   // phone -> rover
constexpr const char* NUS_TX_UUID      = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";   // rover -> phone
constexpr int BLE_TX_DBM      = 9;    // lower (e.g. 3) if the board resets once Bluetooth is up
constexpr int BLE_WRITE_MAX   = 64;   // bytes kept per write (a packet is at most 19)
constexpr int BLE_WRITE_QUEUE = 16;   // writes buffered between the NimBLE task and loop()
constexpr int STATUS_MS       = 1000; // status line heartbeat
constexpr int STATUS_MIN_MS   = 200;  // a changed status waits at least this long after the last one

// ---------- USB serial (spec/features/serial.md) ----------
constexpr unsigned long SERIAL_BAUD = 115200;
constexpr int SERIAL_WAIT_MS  = 1500; // at boot: wait this long for a serial monitor (motors are already off)
constexpr int HEARTBEAT_MS    = 2000;
