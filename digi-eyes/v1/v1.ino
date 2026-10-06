/*
  Digi Eyes - Cosmo-style animated robot eyes + mini smart display
  Seeed Studio XIAO ESP32C3 + 0.96" I2C OLED (SSD1306, 128x64)

  WHAT IT DOES:
  - Power-up / wake: animated eyes for 5 s -> date & time (internet time)
    for 5 s -> screen dims to a branding logo (two sleepy eyes + name).
  - A phone notification wakes the screen: the eyes pop open, then a card
    shows the app icon, sender and message. Unread ones leave a bell badge
    on the dim logo until you look at them.
  - Incoming calls show a ringing handset with the caller's name.
  - Control it from your phone's browser: faces, which screen to show,
    keep-screen-on, test notifications, WiFi settings, notification list.
  - Pair with the free Chronos app (Android) over Bluetooth to get your
    phone's notifications, calls, battery level and time.

  WHY http://192.168.4.1 DIDN'T WORK WHILE INTERNET WAS ON:
  The old sketch made the ESP32 its own WiFi network, which has no
  internet. Phones flag such a network as "no internet" and send traffic
  over mobile data instead, so 192.168.4.1 couldn't be reached. (If the
  hotspot had the same name as your router, the phone may simply have
  joined the router instead of the ESP32.) Now the ESP32 joins your home WiFi like any
  other gadget, so the phone keeps internet AND reaches the control page.
  If home WiFi isn't available it falls back to a "DigiEyes-Setup" hotspot
  with a captive portal: the phone pops up "Sign in to network", which
  opens the control page even while mobile data is on.

  WIRING (XIAO ESP32C3 - use the pad labels printed on the board):
    OLED GND -> GND
    OLED VCC -> 3V3
    OLED SDA -> D4  (GPIO6, the XIAO's hardware I2C SDA)
    OLED SCL -> D5  (GPIO7, the XIAO's hardware I2C SCL)
  Also screw the WiFi antenna onto the board's U.FL connector - without it
  WiFi and Bluetooth are extremely weak.

  LIBRARIES NEEDED (Arduino IDE -> Library Manager):
    - Adafruit GFX Library
    - Adafruit SSD1306
    - ChronosESP32 (by fbiego) - say "Install all" so it also brings in
      NimBLE-Arduino and ESP32Time
    (WiFi, WebServer, DNSServer, ESPmDNS and Preferences come with the
    ESP32 board package already)

  SETUP:
    1. Put your home WiFi name/password in config.h (and check TZ_INFO).
    2. Tools -> Board -> esp32 -> "XIAO_ESP32C3"
       Tools -> Partition Scheme -> "Huge APP (3MB No OTA/1MB SPIFFS)"
       (Bluetooth makes the sketch too big for the default partition.)
       Pick its COM port, Upload.
       (If upload can't connect: hold BOOT, tap RESET, release BOOT, retry.)
    3. Phone on the same home WiFi -> open http://digieyes.local
       If that name doesn't open (some Android phones), press BOOT a few
       times to reach the Info screen, which shows the IP address.
    4. For phone notifications: Chronos app -> Watches -> Pair new watch
       -> "Digi Eyes" (pair from the app, not Android Bluetooth settings).

  BOOT BUTTON (GPIO9):
    - Short press: wake up, then step through the screens
      eyes -> clock -> notifications -> battery -> info -> dim.
      On a notification or call that just popped up, a press dismisses it.
    - Hold ~1 s: power off (light sleep). Press again to power back on.

  FIX NOTES (AP not showing up in WiFi list):
  Root cause was the radio browning out during AP beaconing at full TX power
  on ESP32-C3 Super Mini boards. The fix is to explicitly drop TX power
  right after WiFi.mode() and before connecting / starting the AP. The same
  limit is applied in home-WiFi mode (WIFI_TX_POWER in config.h). This makes
  the brownout-detector-disable hack unnecessary, so it's been removed.

  POWER-OFF NOTES:
  The ESP32 can't cut its own power from software, so "off" here means
  light sleep: OLED is blanked and WiFi + Bluetooth are shut down.
  GPIO9 (BOOT) isn't RTC-capable on the C3, so deep-sleep ext0/ext1 wakeup
  can't use it - light sleep with a plain GPIO wakeup works on any pin
  instead. Waking restarts the chip (the cleanest way to bring WiFi and
  Bluetooth back), which also replays the eyes -> clock wake-up sequence.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <time.h>
#include <sys/time.h>
#include "esp_wifi.h"   // usually pulled in by WiFi.h already, but explicit here
#include "esp_sleep.h"
#include "driver/gpio.h"

#include "config.h"
#include "icons.h"
#include "web_page.h"

#if USE_BLE_CHRONOS
#include <ChronosESP32.h>
#endif

// ---------- OLED / I2C ----------
#define SDA_PIN SDA   // XIAO ESP32C3: pad D4 = GPIO6
#define SCL_PIN SCL   // XIAO ESP32C3: pad D5 = GPIO7
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
#define FULL_CONTRAST 0xCF   // what Adafruit_SSD1306::begin() sets for a 128x64 panel

// ---------- Built-in BOOT button (wake / next screen / power off) ----------
#define BOOT_BUTTON 9          // active LOW
#define HOLD_MS_TO_SLEEP 1000  // how long to hold BOOT before sleeping
#define DEBOUNCE_MS 30

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;
#if USE_BLE_CHRONOS
ChronosESP32 chronos;
#endif

// ---------- Eye geometry ----------
int eyeWidth = 34;
int eyeHeight = 40;
int eyeRadius = 10;
int eyeGap = 12;
int leftEyeX, rightEyeX, eyeY;
int centerY = SCREEN_HEIGHT / 2;

// current face mode, changed by web requests
String currentMode = "normal";
String lastDrawnMode = "";
const char* const FACES[] = { "normal", "happy", "surprised", "angry", "sleepy", "wink", "love", "lookleft", "lookright" };

// ---------- Screens ----------
enum Screen { SCR_EYES, SCR_CLOCK, SCR_NOTIFY, SCR_BATTERY, SCR_INFO, SCR_CALL, SCR_IDLE, SCREEN_COUNT };
const char* const SCREEN_NAMES[SCREEN_COUNT] = { "eyes", "clock", "notify", "battery", "info", "call", "idle" };

Screen screen = SCR_EYES;
Screen drawnScreen = SCR_IDLE;
unsigned long screenSince = 0;   // when the current screen was entered (drives the timeouts)
bool redrawNow = false;          // something on the current screen changed: restart its drawing
bool stayOn = false;             // "keep screen on" from the web page: no timeouts
bool panelDim = false;

// ---------- Notifications ----------
enum NotifKind : uint8_t { NK_BELL, NK_CHAT, NK_MAIL, NK_CALL };
struct Notif {
  uint8_t kind;
  char app[20];
  char title[40];
  char text[200];
  char when[6];   // "HH:MM" when it arrived
};

// Bluetooth callbacks run on the BLE task, so they hand events to loop()
// through a queue instead of touching the display or globals directly.
enum PhoneEventType : uint8_t { EV_NOTIF, EV_CALL_START, EV_CALL_END };
struct PhoneEvent {
  uint8_t type;
  Notif n;
};
QueueHandle_t phoneEvents;

#define NOTIF_HISTORY 5
#define NOTIF_COLS 21        // 128 px / 6 px per character
#define NOTIF_ROWS 5         // message lines visible on the card
#define NOTIF_MAX_LINES 16
#define NOTIF_SCROLL_MS 1200 // time per line when a long message scrolls
Notif notifs[NOTIF_HISTORY];
int notifTotal = 0;          // ever received; the newest is notifAt(0)
int unread = 0;
bool notifAlert = false;     // play the "eyes pop open" reaction before the card
bool notifPopup = false;     // card popped up by itself (vs. reached with the button)
unsigned long notifShownAt = 0;
int notifLines = 0;
char wrapLines[NOTIF_MAX_LINES][NOTIF_COLS + 1];

Notif callInfo;
bool callActive = false;

// ---------- Network ----------
enum NetMode { NET_CONNECTING, NET_STA, NET_AP };
NetMode netMode = NET_CONNECTING;
unsigned long netSince = 0;
String wifiSsid, wifiPass;
bool mdnsStarted = false;

// ---------- Batteries ----------
volatile int phoneBattery = -1;      // %, -1 = unknown. From the Chronos app or the web page
volatile bool phoneCharging = false;
int deviceBattery = -1;              // %, -1 = no battery sensor wired
int deviceMilliVolts = 0;

// ---------- Text helpers ----------
// The OLED font is plain ASCII: keep that, turn "smart" punctuation into
// ASCII, drop emoji, and show any other character as '?'.
void toAscii(const char* in, char* out, size_t outSize) {
  size_t o = 0;
  const uint8_t* p = (const uint8_t*)in;
  while (*p && o + 1 < outSize) {
    uint8_t c = *p;
    if (c < 0x80) {
      out[o++] = (c == '\r' || c == '\t') ? ' ' : (char)c;
      p++;
      continue;
    }
    int len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
    uint32_t cp = (len == 1) ? 0 : (c & (0x3F >> (len - 1)));
    int i = 1;
    for (; i < len && (p[i] & 0xC0) == 0x80; i++) cp = (cp << 6) | (p[i] & 0x3F);
    p += i;

    const char* r;
    if (cp == 0x2018 || cp == 0x2019) r = "'";
    else if (cp == 0x201C || cp == 0x201D) r = "\"";
    else if (cp == 0x2013 || cp == 0x2014) r = "-";
    else if (cp == 0x2026) r = "...";
    else if (cp == 0xA0) r = " ";
    else if (cp >= 0x1F000 || (cp >= 0x2600 && cp <= 0x27BF) || cp == 0xFE0F || cp == 0x200D) r = "";  // emoji
    else r = "?";
    for (; *r && o + 1 < outSize; r++) out[o++] = *r;
  }
  out[o] = 0;
}

// Word-wraps s into wrapLines[], returns the number of lines
int wrapText(const char* s, int cols) {
  int n = 0;
  const char* p = s;
  while (*p && n < NOTIF_MAX_LINES) {
    while (*p == ' ') p++;
    if (!*p) break;
    int len = strlen(p);
    int take = len < cols ? len : cols;
    const char* nl = (const char*)memchr(p, '\n', take);
    if (nl) {
      take = nl - p;
    } else if (len > cols) {
      int sp = take;                    // p[take] is the first character that doesn't fit
      while (sp > 0 && p[sp] != ' ') sp--;
      if (sp > 0) take = sp;            // break at the last space, else cut the long word
    }
    memcpy(wrapLines[n], p, take);
    wrapLines[n][take] = 0;
    n++;
    p += take;
    if (*p == '\n') p++;
  }
  return n;
}

// default 6x8 font, cut with ".." if longer than maxChars
void printClipped(int x, int y, const char* s, int maxChars) {
  display.setCursor(x, y);
  if ((int)strlen(s) <= maxChars) { display.print(s); return; }
  for (int i = 0; i < maxChars - 2; i++) display.write(s[i]);
  display.print("..");
}

// default font, centered inside the column [x0, x0 + w)
void printCenteredIn(int x0, int w, int y, const char* s, uint8_t size) {
  display.setTextSize(size);
  int tw = strlen(s) * 6 * size;
  display.setCursor(x0 + max(0, (w - tw) / 2), y);
  display.print(s);
  display.setTextSize(1);
}

// FreeFont text centered on centerX, y is the baseline
void printCenteredFont(const GFXfont* f, int centerX, int baseline, const char* s) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setFont(f);
  display.getTextBounds(s, 0, baseline, &x1, &y1, &w, &h);
  display.setCursor(centerX - w / 2 - x1, baseline);
  display.print(s);
  display.setFont(nullptr);
}

void drawBitmapScaled(int x, int y, const uint8_t* bmp, int w, int h, int scale) {
  int bytesPerRow = (w + 7) / 8;
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (pgm_read_byte(bmp + j * bytesPerRow + i / 8) & (0x80 >> (i & 7)))
        display.fillRect(x + i * scale, y + j * scale, scale, scale, SSD1306_WHITE);
}

// ---------- Time ----------
bool timeValid() {
  return time(nullptr) > 1700000000;   // anything before Nov 2023 means "never set"
}

bool localNow(struct tm* t) {
  if (!timeValid()) return false;
  time_t now = time(nullptr);
  localtime_r(&now, t);
  return true;
}

void stampTime(char* out, size_t size) {
  struct tm t;
  if (localNow(&t)) strftime(out, size, CLOCK_24H ? "%H:%M" : "%I:%M", &t);
  else out[0] = 0;
}

// ---------- Screen control ----------
void showScreen(Screen s) {
  if (s == SCR_NOTIFY) notifShownAt = millis();
  notifPopup = false;
  screen = s;
  screenSince = millis();
  redrawNow = true;
}

void markRead() {
  unread = 0;
  redrawNow = true;
}

// what a short BOOT press moves to
Screen nextScreen(Screen s) {
  switch (s) {
    case SCR_IDLE:    return SCR_EYES;
    case SCR_EYES:    return SCR_CLOCK;
    case SCR_CLOCK:   return notifTotal > 0 ? SCR_NOTIFY : SCR_BATTERY;
    case SCR_NOTIFY:  return SCR_BATTERY;
    case SCR_BATTERY: return SCR_INFO;
    default:          return SCR_IDLE;   // info, call
  }
}

// eyes -> clock -> dim, everything else -> dim
void checkScreenTimeout() {
  if (screen == SCR_IDLE) return;
  if (stayOn && screen != SCR_CALL) return;

  unsigned long limit;
  switch (screen) {
    case SCR_EYES:   limit = EYES_SCREEN_MS; break;
    case SCR_NOTIFY: limit = NOTIFY_SCREEN_MS + max(0, notifLines - NOTIF_ROWS) * NOTIF_SCROLL_MS; break;
    case SCR_CALL:   limit = CALL_SCREEN_MS; break;
    default:         limit = SCREEN_TIMEOUT_MS;
  }
  if (millis() - screenSince < limit) return;

  if (screen == SCR_CALL) callActive = false;
  showScreen(screen == SCR_EYES ? SCR_CLOCK : SCR_IDLE);
}

// Dimming: contrast alone barely changes an SSD1306, so DIM_DEEP also
// shortens the pre-charge period and lowers VCOMH (values restored on wake).
void setPanelDim(bool dim) {
  if (dim == panelDim) return;
  panelDim = dim;
  if (dim) {
    for (int c = FULL_CONTRAST; c > DIM_CONTRAST; c -= 16) {   // short fade-out
      display.ssd1306_command(SSD1306_SETCONTRAST);
      display.ssd1306_command(c);
      delay(15);
    }
  }
  display.ssd1306_command(SSD1306_SETCONTRAST);
  display.ssd1306_command(dim ? DIM_CONTRAST : FULL_CONTRAST);
  if (DIM_DEEP) {
    display.ssd1306_command(SSD1306_SETPRECHARGE);
    display.ssd1306_command(dim ? 0x11 : 0xF1);
    display.ssd1306_command(SSD1306_SETVCOMDETECT);
    display.ssd1306_command(dim ? 0x00 : 0x40);
  }
}

// ---------- Notifications ----------
const Notif& notifAt(int i) {   // 0 = newest
  return notifs[(notifTotal - 1 - i) % NOTIF_HISTORY];
}

uint8_t kindFromName(String s) {
  s.toLowerCase();
  if (s.indexOf("mail") >= 0) return NK_MAIL;
  if (s.indexOf("call") >= 0 || s.indexOf("phone") >= 0 || s.indexOf("dialer") >= 0) return NK_CALL;
  const char* chats[] = { "chat", "message", "sms", "whatsapp", "telegram", "signal", "messenger", "wechat", "line", "viber", "skype", "discord", "slack" };
  for (const char* c : chats)
    if (s.indexOf(c) >= 0) return NK_CHAT;
  return NK_BELL;
}

const uint8_t* kindIcon(uint8_t kind) {
  switch (kind) {
    case NK_CHAT: return ICON_CHAT16;
    case NK_MAIL: return ICON_MAIL16;
    case NK_CALL: return ICON_CALL16;
    default:      return ICON_BELL16;
  }
}

void addNotification(const Notif& n) {
  Notif& slot = notifs[notifTotal % NOTIF_HISTORY];
  slot = n;
  stampTime(slot.when, sizeof(slot.when));
  notifTotal++;
  unread++;
  notifAlert = true;
  if (screen == SCR_CALL) return;   // don't bury an incoming call under a message
  showScreen(SCR_NOTIFY);
  notifPopup = true;
}

void processPhoneEvents() {
  PhoneEvent e;
  while (xQueueReceive(phoneEvents, &e, 0) == pdTRUE) {
    if (e.type == EV_NOTIF) {
      addNotification(e.n);
    } else if (e.type == EV_CALL_START) {
      callInfo = e.n;
      callActive = true;
      showScreen(SCR_CALL);
    } else {   // EV_CALL_END
      callActive = false;
      if (screen == SCR_CALL) showScreen(SCR_IDLE);
    }
  }
}

// ---------- Chronos app (Bluetooth) ----------
#if USE_BLE_CHRONOS
uint8_t kindFromChronosIcon(int icon) {
  switch (icon) {
    case 0x04: case 0x0B: return NK_MAIL;   // Mail, Gmail
    case 0x03: case 0x08: case 0x09: case 0x0A: case 0x0E: case 0x11: case 0x14:
    case 0x16: case 0x18: case 0x1B: case 0x20: return NK_CHAT;   // SMS, WhatsApp, Telegram, ...
    default: return NK_BELL;
  }
}

void onChronosNotification(Notification n) {
  PhoneEvent e = {};
  e.type = EV_NOTIF;
  e.n.kind = kindFromChronosIcon(n.icon);
  toAscii(n.app.c_str(), e.n.app, sizeof(e.n.app));
  toAscii(n.title.c_str(), e.n.title, sizeof(e.n.title));
  toAscii(n.message.c_str(), e.n.text, sizeof(e.n.text));
  xQueueSend(phoneEvents, &e, 0);
}

void onChronosRinger(String caller, bool ringing) {
  PhoneEvent e = {};
  e.type = ringing ? EV_CALL_START : EV_CALL_END;
  e.n.kind = NK_CALL;
  toAscii(caller.c_str(), e.n.title, sizeof(e.n.title));
  xQueueSend(phoneEvents, &e, 0);
}

void onChronosConfig(Config type, uint32_t a, uint32_t b) {
  if (type == CF_PBAT) {
    phoneCharging = (a == 1);
    phoneBattery = b;
  }
}
#endif

bool phoneLinked() {
#if USE_BLE_CHRONOS
  return chronos.isConnected();
#else
  return false;
#endif
}

// ---------- Device battery ----------
int lipoPercent(int mv) {
  const int pts[][2] = { {4200, 100}, {4100, 90}, {4000, 80}, {3900, 65}, {3800, 50}, {3700, 30}, {3600, 15}, {3500, 5}, {3300, 0} };
  if (mv >= pts[0][0]) return 100;
  for (int i = 1; i < 9; i++)
    if (mv >= pts[i][0])
      return pts[i][1] + (mv - pts[i][0]) * (pts[i - 1][1] - pts[i][1]) / (pts[i - 1][0] - pts[i][0]);
  return 0;
}

void sampleDeviceBattery() {
#if BATTERY_ADC_PIN >= 0
  static unsigned long lastSample = 0;
  if (lastSample && millis() - lastSample < 5000) return;
  lastSample = millis();
  uint32_t mv = 0;
  for (int i = 0; i < 8; i++) mv += analogReadMilliVolts(BATTERY_ADC_PIN);
  deviceMilliVolts = (int)(mv / 8 * BATTERY_DIVIDER);
  int pct = lipoPercent(deviceMilliVolts);
  if (pct != deviceBattery) {
    deviceBattery = pct;
#if USE_BLE_CHRONOS
    chronos.setBattery(pct);   // shows up as the "watch" battery in the app
#endif
  }
#endif
}

// ---------- Network ----------
void loadWifiCredentials() {
  prefs.begin("digieyes", true);
  wifiSsid = prefs.getString("ssid", HOME_WIFI_SSID);
  wifiPass = prefs.getString("pass", HOME_WIFI_PASS);
  prefs.end();
}

void startStation() {
  if (netMode == NET_AP) {
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
  }
  WiFi.setHostname(HOSTNAME);   // must come before WiFi.mode()
  WiFi.mode(WIFI_STA);
  delay(100);
  WiFi.setTxPower(WIFI_TX_POWER);   // the brownout fix (see FIX NOTES)
  WiFi.begin(wifiSsid.c_str(), wifiPass.c_str());
  netMode = NET_CONNECTING;
  netSince = millis();
  Serial.printf("Connecting to WiFi \"%s\"...\n", wifiSsid.c_str());
}

void startSetupAP() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.setTxPower(WIFI_TX_POWER);
  WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS);
  // captive portal: answer every DNS lookup with our own address so the
  // phone shows "Sign in to network" and opens the control page
  dnsServer.start(53, "*", WiFi.softAPIP());
  netMode = NET_AP;
  netSince = millis();
  Serial.printf("Home WiFi not reachable. Setup hotspot \"%s\" (password %s) -> http://%s\n",
                SETUP_AP_SSID, SETUP_AP_PASS, WiFi.softAPIP().toString().c_str());
}

void onWifiConnected() {
  netMode = NET_STA;
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  if (mdnsStarted) MDNS.end();
  mdnsStarted = MDNS.begin(HOSTNAME);
  if (mdnsStarted) MDNS.addService("http", "tcp", 80);
  Serial.printf("WiFi connected. Open http://%s or http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
}

void serviceNetwork() {
  if (netMode == NET_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) onWifiConnected();
    else if (millis() - netSince > WIFI_CONNECT_TIMEOUT_MS) startSetupAP();
  } else if (netMode == NET_AP) {
    dnsServer.processNextRequest();
    // After a power cut the router often boots slower than the ESP32, so
    // keep retrying home WiFi - but never while someone is on the hotspot.
    if (wifiSsid.length() && millis() - netSince > AP_RETRY_MS && WiFi.softAPgetStationNum() == 0) startStation();
  }
  // in NET_STA the WiFi driver reconnects on its own after drop-outs
}

// ---------- Web handlers ----------
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", PAGE_HTML);
}

void handleSet() {
  String m = server.arg("mode");
  for (const char* f : FACES) {
    if (m == f) {
      currentMode = m;
      showScreen(SCR_EYES);
      break;
    }
  }
  server.send(200, "text/plain", "ok");
}

void handleScreen() {
  String s = server.arg("s");
  for (int i = 0; i < SCREEN_COUNT; i++) {
    if (s == SCREEN_NAMES[i] && i != SCR_CALL) {
      if (i == SCR_NOTIFY) markRead();
      showScreen((Screen)i);
      break;
    }
  }
  server.send(200, "text/plain", "ok");
}

void handleStayOn() {
  stayOn = (server.arg("v") == "1");
  screenSince = millis();   // timeouts restart from now when switched back off
  server.send(200, "text/plain", "ok");
}

// GET or POST /notify?app=WhatsApp&title=Mom&text=Hello[&icon=chat|mail|call|bell]
void handleNotify() {
  Notif n = {};
  String app = server.arg("app");
  String text = server.hasArg("text") ? server.arg("text") : server.arg("message");
  if (!app.length()) app = "Phone";
  toAscii(app.c_str(), n.app, sizeof(n.app));
  toAscii(server.arg("title").c_str(), n.title, sizeof(n.title));
  toAscii(text.c_str(), n.text, sizeof(n.text));
  n.kind = kindFromName(server.hasArg("icon") ? server.arg("icon") : app);
  addNotification(n);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

// The control page reports the phone's clock and (Chrome/Android) battery
void handlePhone() {
  if (server.hasArg("bat")) {
    phoneBattery = constrain(server.arg("bat").toInt(), 0, 100);
    phoneCharging = (server.arg("chg") == "1");
  }
  if (server.hasArg("t") && (netMode != NET_STA || !timeValid())) {
    // no internet time (setup hotspot, or NTP not answered yet): use the phone's
    struct timeval tv = { (time_t)strtoll(server.arg("t").c_str(), nullptr, 10), 0 };
    settimeofday(&tv, nullptr);
  }
  server.send(200, "text/plain", "ok");
}

void jsonStr(String& j, const char* s) {
  j += '"';
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') { j += '\\'; j += *s; }
    else if (*s == '\n') j += "\\n";
    else if ((uint8_t)*s < 0x20) j += ' ';
    else j += *s;
  }
  j += '"';
}

void handleStatus() {
  char timeBuf[12] = "", dateBuf[20] = "";
  struct tm t;
  bool ok = localNow(&t);
  if (ok) {
    strftime(timeBuf, sizeof(timeBuf), CLOCK_24H ? "%H:%M" : "%I:%M %p", &t);
    strftime(dateBuf, sizeof(dateBuf), "%a, %d %b %Y", &t);
  }

  String j;
  j.reserve(1800);
  j += "{\"timeOk\":"; j += ok ? "true" : "false";
  j += ",\"time\":"; jsonStr(j, timeBuf);
  j += ",\"date\":"; jsonStr(j, dateBuf);
  j += ",\"net\":\""; j += netMode == NET_STA ? "sta" : netMode == NET_AP ? "ap" : "connecting"; j += '"';
  j += ",\"ssid\":"; jsonStr(j, wifiSsid.c_str());
  j += ",\"ip\":\""; j += WiFi.localIP().toString(); j += '"';
  j += ",\"host\":\"" HOSTNAME "\"";
  j += ",\"rssi\":"; j += (int)WiFi.RSSI();
  j += ",\"bleOn\":"; j += USE_BLE_CHRONOS ? "true" : "false";
  j += ",\"ble\":"; j += phoneLinked() ? "true" : "false";
  j += ",\"pbat\":"; j += phoneBattery;
  j += ",\"pchg\":"; j += phoneCharging ? "true" : "false";
  j += ",\"dbat\":"; j += deviceBattery;
  j += ",\"dmv\":"; j += deviceMilliVolts;
  j += ",\"unread\":"; j += unread;
  j += ",\"face\":"; jsonStr(j, currentMode.c_str());
  j += ",\"screen\":\""; j += SCREEN_NAMES[screen]; j += '"';
  j += ",\"stayOn\":"; j += stayOn ? "true" : "false";
  j += ",\"notifs\":[";
  int count = min(notifTotal, NOTIF_HISTORY);
  for (int i = 0; i < count; i++) {
    const Notif& n = notifAt(i);
    if (i) j += ',';
    j += "{\"app\":"; jsonStr(j, n.app);
    j += ",\"title\":"; jsonStr(j, n.title);
    j += ",\"text\":"; jsonStr(j, n.text);
    j += ",\"when\":"; jsonStr(j, n.when);
    j += '}';
  }
  j += "]}";
  server.send(200, "application/json", j);
}

void handleWifi() {
  String ssid = server.arg("ssid");
  if (!ssid.length()) {
    server.send(400, "text/plain", "WiFi name is required");
    return;
  }
  prefs.begin("digieyes", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", server.arg("pass"));
  prefs.end();
  server.send(200, "text/html",
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<body style='font-family:sans-serif;background:#111;color:#eee;padding:20px'>"
    "<h3>Saved - restarting.</h3><p>Digi Eyes will now join the new WiFi. Connect your phone to "
    "the same WiFi and open http://" HOSTNAME ".local, or press BOOT a few times to see the IP "
    "on the Info screen. If it can't connect, the " SETUP_AP_SSID " hotspot comes back.</p>");
  delay(1000);
  ESP.restart();
}

void handleNotFound() {
  if (netMode == NET_AP) {   // captive portal: send every stray request to the control page
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
    return;
  }
  server.send(404, "text/plain", "not found");
}

// ---------- Drawing helpers ----------
void drawEyes(int offsetX, int offsetY, int leftH, int rightH) {
  display.clearDisplay();
  int lY = eyeY + offsetY + (eyeHeight - leftH) / 2;
  int rY = eyeY + offsetY + (eyeHeight - rightH) / 2;
  display.fillRoundRect(leftEyeX + offsetX, lY, eyeWidth, leftH, eyeRadius, SSD1306_WHITE);
  display.fillRoundRect(rightEyeX + offsetX, rY, eyeWidth, rightH, eyeRadius, SSD1306_WHITE);
  display.display();
}

// true when whatever is being drawn should stop: new screen, new face,
// or fresh content for the current screen
bool interrupted() {
  return redrawNow || screen != drawnScreen || currentMode != lastDrawnMode;
}

// keeps web requests, Bluetooth, the button and screen timeouts running
// during an animation; returns false if the animation should stop
bool stayInMode(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    service();
    if (interrupted()) return false;
    delay(4);
  }
  return true;
}

void blink() {
  for (int h = eyeHeight; h >= 4; h -= 10) { drawEyes(0, 0, h, h); if (!stayInMode(4)) return; }
  for (int h = 4; h <= eyeHeight; h += 10) { drawEyes(0, 0, h, h); if (!stayInMode(4)) return; }
}

void lookAt(int dx, int dy, int holdMs) {
  for (int i = 0; i <= 10; i += 2) { drawEyes((dx*i)/10, (dy*i)/10, eyeHeight, eyeHeight); if (!stayInMode(4)) return; }
  if (!stayInMode(holdMs)) return;
  for (int i = 10; i >= 0; i -= 2) { drawEyes((dx*i)/10, (dy*i)/10, eyeHeight, eyeHeight); if (!stayInMode(4)) return; }
}

void angryBrows() {
  // draw normal eyes then overlay two black diagonal wedges to fake angled eyebrows
  drawEyes(0, 0, eyeHeight, eyeHeight);
  display.fillTriangle(leftEyeX - 4, eyeY - 2, leftEyeX + eyeWidth, eyeY - 2, leftEyeX - 4, eyeY + 14, SSD1306_BLACK);
  display.fillTriangle(rightEyeX + eyeWidth + 4, eyeY - 2, rightEyeX, eyeY - 2, rightEyeX + eyeWidth + 4, eyeY + 14, SSD1306_BLACK);
  display.display();
}

void sleepyEyes() {
  drawEyes(0, 6, eyeHeight / 3, eyeHeight / 3);
}

void winkEyes() {
  drawEyes(0, 0, 6, eyeHeight);
}

void bigEyes() {
  int bigH = eyeHeight + 10, bigW = eyeWidth + 6;
  display.clearDisplay();
  display.fillRoundRect(leftEyeX - 3, eyeY - 5, bigW, bigH, eyeRadius, SSD1306_WHITE);
  display.fillRoundRect(rightEyeX - 3, eyeY - 5, bigW, bigH, eyeRadius, SSD1306_WHITE);
  display.display();
}

void heartEyes() {
  display.clearDisplay();
  drawHeart(leftEyeX + eyeWidth / 2, eyeY + eyeHeight / 2, 16);
  drawHeart(rightEyeX + eyeWidth / 2, eyeY + eyeHeight / 2, 16);
  display.display();
}

void drawHeart(int cx, int cy, int size) {
  int r = size / 3;
  display.fillCircle(cx - r, cy - r / 2, r, SSD1306_WHITE);
  display.fillCircle(cx + r, cy - r / 2, r, SSD1306_WHITE);
  display.fillTriangle(cx - size, cy - r / 3, cx + size, cy - r / 3, cx, cy + size, SSD1306_WHITE);
}

// little battery for the status bar: 13x7 px
void drawSmallBattery(int x, int y, int pct) {
  display.drawRect(x, y, 12, 7, SSD1306_WHITE);
  display.fillRect(x + 12, y + 2, 1, 3, SSD1306_WHITE);
  display.fillRect(x + 1, y + 1, (10 * pct) / 100, 5, SSD1306_WHITE);
}

// bell + unread count ending at x = right; returns the left edge
int drawBellBadge(int right, int y) {
  char buf[12];
  if (unread > 9) strcpy(buf, "9+");
  else snprintf(buf, sizeof(buf), "%d", unread);
  int w = strlen(buf) * 6;
  display.setCursor(right - w, y);
  display.print(buf);
  int bx = right - w - 9;
  display.drawBitmap(bx, y, ICON_BELL8, 8, 8, SSD1306_WHITE);
  return bx;
}

// top row of the clock: WiFi + Bluetooth on the left, phone battery + unread on the right
void drawStatusBar() {
  int x = 0;
  if (netMode == NET_STA && WiFi.status() == WL_CONNECTED) {
    display.drawBitmap(x, 0, ICON_WIFI8, 8, 8, SSD1306_WHITE);
    x += 11;
  } else if (netMode == NET_AP) {
    display.setCursor(x, 0);
    display.print("AP");
    x += 15;
  }
  if (phoneLinked()) display.drawBitmap(x, 0, ICON_BT8, 8, 8, SSD1306_WHITE);

  int r = SCREEN_WIDTH;
  if (unread > 0) r = drawBellBadge(r, 0) - 5;
  int pct = phoneBattery;
  if (pct >= 0) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    r -= strlen(buf) * 6;
    display.setCursor(r, 0);
    display.print(buf);
    drawSmallBattery(r - 15, 0, pct);
  }
}

// ---------- Screens ----------
void drawClock() {
  display.clearDisplay();
  display.setTextSize(1);
  drawStatusBar();

  struct tm t;
  bool ok = localNow(&t);
  char hm[16], ampm[3] = "", secs[12] = "", date[24];
  if (ok) {
    int h = t.tm_hour;
    if (!CLOCK_24H) {
      strcpy(ampm, h < 12 ? "AM" : "PM");
      h %= 12;
      if (h == 0) h = 12;
    }
    snprintf(hm, sizeof(hm), CLOCK_24H ? "%02d:%02d" : "%d:%02d", h, t.tm_min);
    snprintf(secs, sizeof(secs), "%02d", t.tm_sec);
    strftime(date, sizeof(date), "%a, %d %b %Y", &t);
  } else {
    strcpy(hm, "--:--");
    if (netMode == NET_AP) snprintf(date, sizeof(date), "Join %s", SETUP_AP_SSID);
    else if (netMode == NET_CONNECTING) strcpy(date, "Connecting WiFi...");
    else strcpy(date, "Syncing time...");
  }

  // big HH:MM, with AM/PM above the seconds in a small column on its right
  int16_t x1, y1;
  uint16_t w, h;
  display.setFont(&FreeSansBold18pt7b);
  display.getTextBounds(hm, 0, 42, &x1, &y1, &w, &h);
  int total = w + 4 + 12;
  int x = (SCREEN_WIDTH - total) / 2;
  display.setCursor(x - x1, 42);
  display.print(hm);
  display.setFont(nullptr);
  display.setCursor(x + w + 4, 18);
  display.print(ampm);
  display.setCursor(x + w + 4, 35);
  display.print(secs);

  printCenteredIn(0, SCREEN_WIDTH, 54, date, 1);
  display.display();
}

void drawNotifCard(const Notif& n, int top) {
  display.clearDisplay();
  display.setTextSize(1);
  display.drawBitmap(0, 0, kindIcon(n.kind), 16, 16, SSD1306_WHITE);
  if (n.when[0]) {
    printClipped(20, 0, n.app, 13);
    display.setCursor(SCREEN_WIDTH - 30, 0);
    display.print(n.when);
  } else {
    printClipped(20, 0, n.app, 18);
  }
  printClipped(20, 9, n.title, 18);
  display.drawFastHLine(0, 18, SCREEN_WIDTH, SSD1306_WHITE);

  for (int i = 0; i < NOTIF_ROWS && top + i < notifLines; i++) {
    display.setCursor(0, 21 + i * 8);
    display.print(wrapLines[top + i]);
  }
  if (notifLines > NOTIF_ROWS) {   // scrollbar on the right edge
    int track = SCREEN_HEIGHT - 21;
    int thumb = max(4, track * NOTIF_ROWS / notifLines);
    int y = 21 + (track - thumb) * top / (notifLines - NOTIF_ROWS);
    display.drawFastVLine(SCREEN_WIDTH - 1, y, thumb, SSD1306_WHITE);
  }
  display.display();
}

// the "you've got something" reaction: eyes pop wide open and blink twice
void alertEyes() {
  for (int i = 0; i < 2; i++) {
    bigEyes();
    if (!stayInMode(350)) return;
    drawEyes(0, 0, 6, 6);
    if (!stayInMode(90)) return;
  }
  bigEyes();
  stayInMode(300);
}

void runNotify() {
  if (notifTotal == 0) {
    display.clearDisplay();
    display.drawBitmap((SCREEN_WIDTH - 16) / 2, 12, ICON_BELL16, 16, 16, SSD1306_WHITE);
    printCenteredIn(0, SCREEN_WIDTH, 40, "No notifications", 1);
    display.display();
    stayInMode(500);
    return;
  }
  if (notifAlert) {
    notifAlert = false;
    alertEyes();
    if (interrupted()) return;
    notifShownAt = millis();
    screenSince = millis();   // the card gets its full time after the animation
  }

  const Notif& n = notifAt(0);
  notifLines = wrapText(n.text[0] ? n.text : n.title, NOTIF_COLS);
  // long messages: hold at the top for a moment, then scroll a line at a time
  int top = 0;
  unsigned long shown = millis() - notifShownAt;
  if (notifLines > NOTIF_ROWS && shown > 2000)
    top = min(notifLines - NOTIF_ROWS, (int)((shown - 2000) / NOTIF_SCROLL_MS) + 1);
  drawNotifCard(n, top);
  stayInMode(150);
}

void runCall() {
  // handset buzzes twice, then rests: like a phone ringing
  for (int i = 0; i < 6; i++) {
    display.clearDisplay();
    int shake = (i < 4) ? ((i % 2) ? 2 : -2) : 0;
    drawBitmapScaled(6 + shake, 16, ICON_CALL16, 16, 16, 2);
    display.setTextSize(1);
    display.setCursor(46, 12);
    display.print("Incoming call");
    int lines = wrapText(callInfo.title[0] ? callInfo.title : "Unknown", 13);
    for (int l = 0; l < lines && l < 3; l++) {
      display.setCursor(46, 28 + l * 10);
      display.print(wrapLines[l]);
    }
    display.display();
    if (!stayInMode(i < 4 ? 90 : 450)) return;
  }
}

void drawBatteryColumn(int x0, const char* label, int pct, const char* sub) {
  printCenteredIn(x0, 64, 0, label, 1);
  int bx = x0 + 10, by = 12;
  display.drawRoundRect(bx, by, 40, 18, 3, SSD1306_WHITE);
  display.fillRect(bx + 40, by + 5, 3, 8, SSD1306_WHITE);
  if (pct >= 0) display.fillRect(bx + 3, by + 3, (34 * pct) / 100, 12, SSD1306_WHITE);
  char buf[12];
  if (pct >= 0) snprintf(buf, sizeof(buf), "%d%%", pct);
  else strcpy(buf, "--");
  printCenteredIn(x0, 64, 36, buf, 2);
  printCenteredIn(x0, 64, 56, sub, 1);
}

void drawBatteryScreen() {
  display.clearDisplay();
  int pb = phoneBattery;
  drawBatteryColumn(0, "PHONE", pb, pb < 0 ? "no data" : (phoneCharging ? "charging" : ""));
  char sub[16] = "USB power";
  if (deviceBattery >= 0) snprintf(sub, sizeof(sub), "%d.%02dV", deviceMilliVolts / 1000, (deviceMilliVolts % 1000) / 10);
  drawBatteryColumn(64, "DEVICE", deviceBattery, sub);
  display.drawFastVLine(63, 2, 60, SSD1306_WHITE);
  display.display();
}

void drawInfo() {
  display.clearDisplay();
  display.setTextSize(1);
  char line[48];
  if (netMode == NET_AP) {
    printClipped(0, 0, "No home WiFi. Setup:", 21);
    snprintf(line, sizeof(line), "WiFi %s", SETUP_AP_SSID);
    printClipped(0, 11, line, 21);
    snprintf(line, sizeof(line), "Pass %s", SETUP_AP_PASS);
    printClipped(0, 22, line, 21);
    snprintf(line, sizeof(line), "Open %s", WiFi.softAPIP().toString().c_str());
    printClipped(0, 33, line, 21);
  } else {
    snprintf(line, sizeof(line), "WiFi %s", wifiSsid.c_str());
    printClipped(0, 0, line, 21);
    if (netMode == NET_STA) snprintf(line, sizeof(line), "IP %s", WiFi.localIP().toString().c_str());
    else strcpy(line, "Connecting...");
    printClipped(0, 11, line, 21);
    printClipped(0, 22, "http://" HOSTNAME ".local", 21);
  }
#if USE_BLE_CHRONOS
  snprintf(line, sizeof(line), "BT %s: %s", BLE_NAME, phoneLinked() ? "linked" : "waiting");
  printClipped(0, 44, line, 21);
#endif
  printClipped(0, 56, "Hold BOOT 1s = off", 21);
  display.display();
}

// branding: two small eyes over the name (or your own bitmap, see icons.h)
void drawBrandLogo(int dx, int dy, bool eyesClosed) {
  if (CUSTOM_LOGO_W > 0) {
    display.drawBitmap((SCREEN_WIDTH - CUSTOM_LOGO_W) / 2 + dx, (SCREEN_HEIGHT - CUSTOM_LOGO_H) / 2 + dy,
                       CUSTOM_LOGO, CUSTOM_LOGO_W, CUSTOM_LOGO_H, SSD1306_WHITE);
    return;
  }
  int ew = 18, eh = eyesClosed ? 3 : 22, gap = 8;
  int x0 = (SCREEN_WIDTH - (2 * ew + gap)) / 2 + dx;
  int y0 = 8 + dy + (22 - eh) / 2;
  display.fillRoundRect(x0, y0, ew, eh, eyesClosed ? 1 : 6, SSD1306_WHITE);
  display.fillRoundRect(x0 + ew + gap, y0, ew, eh, eyesClosed ? 1 : 6, SSD1306_WHITE);
  printCenteredFont(&FreeSansBold9pt7b, SCREEN_WIDTH / 2 + dx, 50 + dy, BRAND_NAME);
}

void drawIdle(bool eyesClosed) {
  // nudge everything by a couple of pixels every minute so the static
  // logo doesn't burn into the OLED
  const int8_t SHIFT[][2] = { {0, 0}, {2, 1}, {-2, 2}, {1, -2}, {-1, -1}, {2, -1}, {-2, 0}, {0, 2} };
  int s = (millis() / 60000) % 8;
  int dx = SHIFT[s][0], dy = SHIFT[s][1];

  display.clearDisplay();
  display.setTextSize(1);
  drawBrandLogo(dx, dy, eyesClosed);
  if (unread > 0) drawBellBadge(125 + dx, 54 + dy);
  display.display();
}

void runIdle() {
  drawIdle(false);
  if (!stayInMode(random(4000, 9000))) return;
  drawIdle(true);   // a slow sleepy blink now and then
  stayInMode(160);
}

// ---------- Mode runners (each runs until currentMode changes) ----------
void runNormal() {
  drawEyes(0, 0, eyeHeight, eyeHeight);
  if (!stayInMode(700)) return;
  blink();
  if (interrupted()) return;
  int r = random(0, 4);
  if (r == 0) lookAt(-15, 0, 200);
  else if (r == 1) lookAt(15, 0, 200);
  else if (r == 2) lookAt(0, -8, 150);
  // r==3: just blink again, no look
}

void runHappy() {
  for (int h = eyeHeight; h >= eyeHeight/2; h -= 8) { drawEyes(0, eyeHeight/4, h, h); if (!stayInMode(6)) return; }
  if (!stayInMode(400)) return;
  for (int h = eyeHeight/2; h <= eyeHeight; h += 8) { drawEyes(0, eyeHeight/4, h, h); if (!stayInMode(6)) return; }
  stayInMode(300);
}

void runSurprised() {
  bigEyes();
  stayInMode(400);
}

void runAngry() {
  angryBrows();
  stayInMode(400);
}

void runSleepy() {
  sleepyEyes();
  stayInMode(400);
}

void runWink() {
  winkEyes();
  stayInMode(400);
}

void runLove() {
  heartEyes();
  stayInMode(400);
}

void runLookLeft() {
  lookAt(-18, 0, 500);
}

void runLookRight() {
  lookAt(18, 0, 500);
}

void runFace() {
  if (currentMode == "normal") runNormal();
  else if (currentMode == "happy") runHappy();
  else if (currentMode == "surprised") runSurprised();
  else if (currentMode == "angry") runAngry();
  else if (currentMode == "sleepy") runSleepy();
  else if (currentMode == "wink") runWink();
  else if (currentMode == "love") runLove();
  else if (currentMode == "lookleft") runLookLeft();
  else if (currentMode == "lookright") runLookRight();
  else runNormal();
}

// ---------- BOOT button (GPIO9) ----------
// Non-blocking check: called from service(), so presses are caught even
// mid-animation. Short press = wake / next screen, hold = power off.
unsigned long buttonPressStart = 0;
bool buttonHeld = false;

void onShortPress() {
  if (screen == SCR_NOTIFY) markRead();     // they've seen it
  if (screen == SCR_CALL) callActive = false;
  // a press on a notification that just popped up dismisses it
  Screen next = notifPopup ? SCR_IDLE : nextScreen(screen);
  if (next == SCR_NOTIFY) markRead();
  showScreen(next);
}

void checkButton() {
  bool pressed = (digitalRead(BOOT_BUTTON) == LOW);

  if (pressed && !buttonHeld) {
    buttonHeld = true;
    buttonPressStart = millis();
  } else if (pressed && buttonHeld) {
    if (millis() - buttonPressStart >= HOLD_MS_TO_SLEEP) {
      goToSleep();  // does not return - the chip restarts after waking
    }
  } else if (!pressed && buttonHeld) {
    buttonHeld = false;
    if (millis() - buttonPressStart >= DEBOUNCE_MS) onShortPress();
  }
}

void goToSleep() {
  // Wait for the current long-press to be released FIRST. The wakeup below
  // is level-triggered (LOW), so if we entered sleep while still held down,
  // the wake condition is already true and it wakes instantly - which is
  // the "flickers off then immediately back on" bug.
  while (digitalRead(BOOT_BUTTON) == LOW) delay(10);
  delay(50); // simple debounce after release

  display.clearDisplay();
  display.display();                          // blank the OLED before sleeping
  display.ssd1306_command(SSD1306_DISPLAYOFF); // panel off, saves a little more power

#if USE_BLE_CHRONOS
  chronos.stop();
#endif
  if (mdnsStarted) MDNS.end();
  if (netMode == NET_AP) WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  gpio_wakeup_enable(GPIO_NUM_9, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  esp_light_sleep_start();   // now genuinely blocks until BOOT is pressed again

  // ---------- execution resumes here after waking ----------
  // GPIO9 is also a boot-mode pin: wait for release so the restart below
  // boots the sketch and not the USB download mode
  while (digitalRead(BOOT_BUTTON) == LOW) delay(10);
  delay(50);
  ESP.restart();
}

// ---------- Main ----------
// Everything that must keep running no matter what is on screen
void service() {
  server.handleClient();
  serviceNetwork();
  checkButton();
#if USE_BLE_CHRONOS
  chronos.loop();
  static bool wasLinked = false;
  if (phoneLinked() != wasLinked) {
    wasLinked = !wasLinked;
    if (wasLinked && screen == SCR_IDLE) showScreen(SCR_CLOCK);   // show the new Bluetooth icon
  }
#endif
  processPhoneEvents();
  sampleDeviceBattery();
  checkScreenTimeout();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  // display.begin() only allocates the buffer - it doesn't check the OLED
  // actually answers on I2C - so probe the address first
  Wire.beginTransmission(SCREEN_ADDRESS);
  bool oledFound = (Wire.endTransmission() == 0);

  if (!oledFound || !display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    // repeat the message: the XIAO's USB serial usually isn't attached yet
    // when setup() runs, so a single print would never be seen
    while (true) {
      Serial.println("OLED not found! Check wiring: SDA -> D4, SCL -> D5, VCC -> 3V3, GND -> GND");
      delay(2000);
    }
  }
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);

  int totalW = eyeWidth * 2 + eyeGap;
  leftEyeX = (SCREEN_WIDTH - totalW) / 2;
  rightEyeX = leftEyeX + eyeWidth + eyeGap;
  eyeY = centerY - eyeHeight / 2;

  display.clearDisplay();
  display.display();
  randomSeed(esp_random());

  setenv("TZ", TZ_INFO, 1);   // local time works even before (or without) internet time
  tzset();
  phoneEvents = xQueueCreate(4, sizeof(PhoneEvent));

  loadWifiCredentials();
  if (wifiSsid.length()) startStation();
  else startSetupAP();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/screen", handleScreen);
  server.on("/stayon", handleStayOn);
  server.on("/notify", handleNotify);
  server.on("/phone", handlePhone);
  server.on("/status", handleStatus);
  server.on("/wifi", HTTP_POST, handleWifi);
  server.onNotFound(handleNotFound);
  server.begin();

#if USE_BLE_CHRONOS
  chronos.setName(BLE_NAME);
  chronos.setNotificationCallback(onChronosNotification);
  chronos.setRingerCallback(onChronosRinger);
  chronos.setConfigurationCallback(onChronosConfig);
  chronos.begin();
  chronos.set24Hour(CLOCK_24H);
  chronos.setNotifyBattery(true);   // ask the app to keep sending the phone's battery level
  Serial.printf("Bluetooth ready: pair \"%s\" from the Chronos app.\n", BLE_NAME);
#endif
  Serial.println("Short-press BOOT to wake / change screen. Hold ~1s to power off.");

  showScreen(SCR_EYES);
}

void loop() {
  service();
  setPanelDim(screen == SCR_IDLE);
  redrawNow = false;
  drawnScreen = screen;
  lastDrawnMode = currentMode;

  switch (screen) {
    case SCR_EYES:    runFace(); break;
    case SCR_CLOCK:   drawClock(); stayInMode(200); break;
    case SCR_NOTIFY:  runNotify(); break;
    case SCR_BATTERY: drawBatteryScreen(); stayInMode(500); break;
    case SCR_INFO:    drawInfo(); stayInMode(500); break;
    case SCR_CALL:    runCall(); break;
    default:          runIdle(); break;
  }
}
