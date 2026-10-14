// Rover C3: a Bluetooth-controlled tracked rover. setup() / loop() wire the modules together
// (spec/core/architecture.md); every pin, limit and timing is in include/config.h.
#include <Arduino.h>
#include <esp_system.h>

#include "ble_link.h"
#include "config.h"
#include "drive.h"
#include "motors.h"
#include "protocol.h"
#include "serial_cmd.h"

static const DriveConfig DRIVE_CONFIG = {SPEED_LEVELS_PCT, SPEED_LEVELS, START_LEVEL, SLEW_STEP_PCT, JOY_TIMEOUT_MS};
static Drive drive(DRIVE_CONFIG);
static PacketParser parser;
static bool linked = false;
static uint32_t lastTick = 0;
static uint32_t lastStatusAt = 0;
static uint32_t lastHeartbeat = 0;
static char lastStatus[40] = "";

// ---------- Boot ----------
static const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power-on";
    case ESP_RST_EXT: return "reset pin";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep";
    case ESP_RST_BROWNOUT: return "BROWNOUT (voltage dip: spec/guides/troubleshooting.md)";
    default: return "other";
  }
}

void setup() {
  motorsBegin();   // IN1..IN4 LOW before anything else: no twitch at power-on

  Serial.begin(SERIAL_BAUD);
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.setTxTimeoutMs(0);   // on the battery there's no USB host: printing must never stall the loop
#endif
  for (uint32_t t = millis(); !Serial && millis() - t < (uint32_t)SERIAL_WAIT_MS;) delay(10);
  Serial.printf("\n%s boot  fw %s  protocol v%d  reset: %s\n", DEVICE_NAME, FW_VERSION, PROTOCOL_VERSION, resetReason());

  bleBegin();
  Serial.println("Bluetooth advertising. Serial: h for the bench commands.");
}

// ---------- Phone ----------
static void checkLink() {
  bool now = bleConnected();
  if (now == linked) return;
  linked = now;
  lastStatus[0] = 0;   // a new link gets the status at once
  if (linked) {
    Serial.println("Bluetooth connected");
    return;
  }
  drive.linkLost();   // failsafe: stop at once, no ramp
  motorsSet(0, 0);
  parser.reset();     // half a packet from the old link must not join the next one
  Serial.println("Bluetooth disconnected: stopped");
}

static void readPhone(uint32_t now) {
  uint8_t buf[BLE_WRITE_MAX + 1];
  size_t n;
  while ((n = bleRead(buf, BLE_WRITE_MAX)) > 0) {
    if (buf[0] != '!' && !parser.busy()) {   // typed text (Bluefruit UART): echoed, the link check
      buf[n] = 0;
      bleSend((const char*)buf);
      continue;
    }
    Packet p;
    for (size_t i = 0; i < n; i++)
      if (parser.feed(buf[i], p)) drive.packet(p, now);
  }
}

// On subscribe: the hello. Then the status line on every change (at most every STATUS_MIN_MS)
// and every STATUS_MS anyway (spec/core/protocol.md#rover--phone-tx).
static void sendStatus(uint32_t now) {
  if (!linked) return;
  if (bleTakeSubscribed()) {
    char hello[48];
    formatHello(hello, sizeof hello, DEVICE_NAME, FW_VERSION, PROTOCOL_VERSION);
    bleSend(hello);
    lastStatus[0] = 0;
  }
  char line[sizeof lastStatus];
  formatStatus(line, sizeof line, drive.level(), drive.targetLeft(), drive.targetRight());
  uint32_t since = now - lastStatusAt;
  bool changed = strcmp(line, lastStatus) != 0;
  if ((changed && since >= (uint32_t)STATUS_MIN_MS) || since >= (uint32_t)STATUS_MS) {
    bleSend(line);
    strcpy(lastStatus, line);
    lastStatusAt = now;
  }
}

// ---------- Serial heartbeat ----------
static void heartbeat(uint32_t now) {
  if (now - lastHeartbeat < (uint32_t)HEARTBEAT_MS) return;
  lastHeartbeat = now;
  Serial.printf("up %lus  link %s  spd %d  target %d/%d  out %d/%d\n", (unsigned long)(now / 1000),
                linked ? "on" : "off", drive.level() + 1, drive.targetLeft(), drive.targetRight(),
                drive.left(), drive.right());
}

void loop() {
  uint32_t now = millis();
  checkLink();
  readPhone(now);
  serialPoll(drive);
  if (now - lastTick >= (uint32_t)CONTROL_TICK_MS) {
    lastTick = now;
    if (drive.tick(now)) Serial.println("Failsafe: no joystick packet in time, stopped");
    motorsSet(drive.left(), drive.right());
  }
  sendStatus(now);
  heartbeat(now);
}
