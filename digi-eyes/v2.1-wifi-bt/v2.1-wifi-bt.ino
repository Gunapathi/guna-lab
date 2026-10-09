/*
  Digi Eyes (06-10 version) - Pebble-style animated face + mini smart display
  Seeed Studio XIAO ESP32C3 + 0.96" I2C OLED (SSD1306, 128x64)

  NEW IN THIS VERSION: the old rounded-rectangle eyes are replaced by a
  cute face (eyes, blush, mouth and little props) with these animations:
    normal    - looks around, blinks, breathes, smiles now and then
    happy     - ^ ^ eyes, big smile, blush, balloon, hops and sparkles
    snoring   - closed eyes, breathing mouth, nose bubble that pops, z Z Z
    angry     - slanted eyes and brows, shaking, gritted teeth, anger mark
    love      - beating heart eyes, hearts floating up
    alone     - sad eyes, a tear, looks around for someone, rain cloud
    confused  - one eye squints, head cocks side to side, "?" marks
    hangover  - heavy eyelids, swaying, "hic!", spinning spiral eyes
    surprised - big round eyes, "O" mouth, "!" (also used for notifications)
  Every frame is drawn from the time since the face started, so a new face
  or screen takes over within one frame (~45 ms).

  WHAT IT DOES:
  - Power-up / wake: animated face for 5 s -> date & time (internet time)
    for 5 s -> screen dims to a branding logo (a little face + name).
    A face picked on the control page stays up for FACE_SCREEN_MS instead.
  - A phone notification wakes the screen: the face looks surprised, then a card
    shows the app icon, sender and message. Unread ones leave a bell badge
    on the dim logo until you look at them.
  - Incoming calls show a ringing handset with the caller's name.
  - Control it from your phone's browser: faces, which screen to show,
    keep-screen-on, date & time, test notifications and calls, WiFi
    settings, notification list - plus a live copy of the OLED.
  - Pair with the free Chronos app (Android) over Bluetooth to get your
    phone's notifications, calls, battery level and time.

  CONNECTING WITHOUT A ROUTER OR INTERNET (HOTSPOT_ONLY in config.h):
  The ESP32 runs its own "DigiEyes" hotspot, which has no internet. Phones
  flag such a network as "no internet" and send the browser's traffic over
  mobile data instead, so a typed http://192.168.4.1 (or .local name) goes
  nowhere. Three things get around that:
    1. Captive portal: every DNS lookup is answered with our own address
       and every unknown page is redirected to the control page, so after
       joining, the phone pops up "Sign in to network" and opens the
       control page inside the hotspot, even while mobile data is on.
    2. The OLED shows QR codes: first one joins the hotspot, and once a
       phone is on it, the second one opens http://192.168.4.1.
    3. For a normal browser tab (Chrome etc.): turn mobile data OFF while
       you use it, then 192.168.4.1 and digieyes.local open fine. Laptops
       reach 192.168.4.1 as soon as they join the hotspot.
  The device has no internet time in this mode, so the control page hands
  it your phone's clock every time it opens.
  With HOTSPOT_ONLY false the ESP32 joins your home WiFi instead (phone keeps
  internet AND reaches the page) and only falls back to the hotspot.

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
    (WiFi, WebServer, DNSServer, ESPmDNS, Preferences and the QR code
    generator come with the ESP32 board package already)

  SETUP:
    1. In config.h pick HOTSPOT_ONLY (and its hotspot name/password) or put
       your home WiFi name/password there. Check TZ_INFO too.
    2. Tools -> Board -> esp32 -> "XIAO_ESP32C3"
       Tools -> Partition Scheme -> "Huge APP (3MB No OTA/1MB SPIFFS)"
       (Bluetooth makes the sketch too big for the default partition.)
       Pick its COM port, Upload.
       (If upload can't connect: hold BOOT, tap RESET, release BOOT, retry.)
    3. Hotspot: after the eyes, the OLED shows a QR code - scan it with the
       phone camera to join "DigiEyes", then scan the next QR (or tap the
       "Sign in to network" pop-up) to open the control page.
       Home WiFi: phone on the same WiFi -> open http://digieyes.local
       If that name doesn't open (some Android phones), press BOOT a few
       times to reach the Info screen, which shows the IP address.
    4. For phone notifications: Chronos app -> Watches -> Pair new watch
       -> "Digi Eyes" (pair from the app, not Android Bluetooth settings).

  BOOT BUTTON (GPIO9):
    - Short press: wake up, then step through the screens
      eyes -> clock -> notifications -> battery -> info -> (hotspot QR) -> dim.
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
#include <qrcode.h>     // Espressif's QR generator, part of the ESP32 core

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

// ---------- Face ----------
// current face mode, changed by web requests
String currentMode = "normal";
String lastDrawnMode = "";
enum FaceId { F_NORMAL, F_HAPPY, F_SNORING, F_ANGRY, F_LOVE, F_ALONE, F_CONFUSED, F_HANGOVER, F_SURPRISED, FACE_COUNT };
const char* const FACES[FACE_COUNT] = { "normal", "happy", "snoring", "angry", "love", "alone", "confused", "hangover", "surprised" };
bool facePicked = false;   // chosen on the control page: stays up for FACE_SCREEN_MS

// ---------- Screens ----------
enum Screen { SCR_EYES, SCR_CLOCK, SCR_NOTIFY, SCR_BATTERY, SCR_INFO, SCR_CONNECT, SCR_CALL, SCR_IDLE, SCREEN_COUNT };
const char* const SCREEN_NAMES[SCREEN_COUNT] = { "eyes", "clock", "notify", "battery", "info", "connect", "call", "idle" };

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
const IPAddress AP_IP(192, 168, 4, 1);
unsigned long lastWebPoll = 0;   // when an open control page last asked for /status

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
  facePicked = false;
  screen = s;
  screenSince = millis();
  redrawNow = true;
}

void markRead() {
  unread = 0;
  redrawNow = true;
}

// a control page is open somewhere (it asks for /status every few seconds)
bool browserOpen() {
  return lastWebPoll && millis() - lastWebPoll < 10000;
}

// what a short BOOT press moves to
Screen nextScreen(Screen s) {
  switch (s) {
    case SCR_IDLE:    return SCR_EYES;
    case SCR_EYES:    return SCR_CLOCK;
    case SCR_CLOCK:   return notifTotal > 0 ? SCR_NOTIFY : SCR_BATTERY;
    case SCR_NOTIFY:  return SCR_BATTERY;
    case SCR_BATTERY: return SCR_INFO;
    case SCR_INFO:    return netMode == NET_AP ? SCR_CONNECT : SCR_IDLE;
    default:          return SCR_IDLE;   // connect, call
  }
}

// eyes -> clock (or the hotspot QR codes while nobody has the page open) -> dim,
// everything else -> dim
void checkScreenTimeout() {
  if (screen == SCR_IDLE) return;
  if (stayOn && screen != SCR_CALL) return;

  unsigned long limit;
  switch (screen) {
    case SCR_EYES:    limit = facePicked ? FACE_SCREEN_MS : EYES_SCREEN_MS; break;
    case SCR_NOTIFY:  limit = NOTIFY_SCREEN_MS + max(0, notifLines - NOTIF_ROWS) * NOTIF_SCROLL_MS; break;
    case SCR_CALL:    limit = CALL_SCREEN_MS; break;
    case SCR_CONNECT: limit = CONNECT_SCREEN_MS; break;
    default:          limit = SCREEN_TIMEOUT_MS;
  }
  if (millis() - screenSince < limit) return;

  if (screen == SCR_CALL) callActive = false;
  Screen next = SCR_IDLE;
  if (screen == SCR_EYES) next = (netMode == NET_AP && !browserOpen()) ? SCR_CONNECT : SCR_CLOCK;
  showScreen(next);
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

void startHotspot() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  delay(100);
  WiFi.setTxPower(WIFI_TX_POWER);
  // the last argument makes DHCP hand out our own address as the DNS server,
  // so the catch-all DNS below really receives the phone's lookups
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0), IPAddress((uint32_t)0), AP_IP);
  WiFi.softAP(HOTSPOT_SSID, HOTSPOT_PASS);
  // captive portal: answer every DNS lookup with our own address so the
  // phone shows "Sign in to network" and opens the control page
  dnsServer.start(53, "*", AP_IP);
  // laptops resolve http://digieyes.local through this; phones get it from the DNS above
  if (mdnsStarted) MDNS.end();
  mdnsStarted = MDNS.begin(HOSTNAME);
  if (mdnsStarted) MDNS.addService("http", "tcp", 80);
  netMode = NET_AP;
  netSince = millis();
  Serial.printf("%s hotspot \"%s\" (password %s) -> http://%s\n", HOTSPOT_ONLY ? "Own" : "Home WiFi not reachable. Fallback",
                HOTSPOT_SSID, HOTSPOT_PASS[0] ? HOTSPOT_PASS : "none", WiFi.softAPIP().toString().c_str());
}

void onWifiConnected() {
  netMode = NET_STA;
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  if (mdnsStarted) MDNS.end();
  mdnsStarted = MDNS.begin(HOSTNAME);
  if (mdnsStarted) MDNS.addService("http", "tcp", 80);
  Serial.printf("WiFi connected. Open http://%s or http://%s.local\n", WiFi.localIP().toString().c_str(), HOSTNAME);
}

// A phone joining the hotspot brings up the QR codes again (the next one
// opens the control page), unless a page is already open or something
// more important is on screen
void watchHotspotJoins() {
  static unsigned long lastCheck = 0;
  static int lastCount = 0;
  if (millis() - lastCheck < 500) return;
  lastCheck = millis();
  int n = WiFi.softAPgetStationNum();
  if (n > lastCount && !browserOpen() && (screen == SCR_IDLE || screen == SCR_CLOCK || screen == SCR_CONNECT))
    showScreen(SCR_CONNECT);
  lastCount = n;
}

void serviceNetwork() {
  if (netMode == NET_CONNECTING) {
    if (WiFi.status() == WL_CONNECTED) onWifiConnected();
    else if (millis() - netSince > WIFI_CONNECT_TIMEOUT_MS) startHotspot();
  } else if (netMode == NET_AP) {
    dnsServer.processNextRequest();
    // After a power cut the router often boots slower than the ESP32, so
    // keep retrying home WiFi - but never while someone is on the hotspot.
    if (!HOTSPOT_ONLY && wifiSsid.length() && millis() - netSince > AP_RETRY_MS && WiFi.softAPgetStationNum() == 0) startStation();
    watchHotspotJoins();
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
      facePicked = true;
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

// GET /call?name=Mom rings the call screen, /call?end=1 hangs up
// (try it without a phone paired over Bluetooth)
void handleCall() {
  if (server.hasArg("end")) {
    callActive = false;
    if (screen == SCR_CALL) showScreen(SCR_IDLE);
  } else {
    callInfo = Notif();
    callInfo.kind = NK_CALL;
    toAscii(server.arg("name").c_str(), callInfo.title, sizeof(callInfo.title));
    callActive = true;
    showScreen(SCR_CALL);
  }
  server.send(200, "text/plain", "ok");
}

// the OLED's 1 KB frame buffer, drawn by the live screen copy on the control page
void handleFrame() {
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (PGM_P)display.getBuffer(), SCREEN_WIDTH * SCREEN_HEIGHT / 8);
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
  // a page just opened while the QR codes are up: say hello and get out of the way
  if (!browserOpen() && screen == SCR_CONNECT) {
    currentMode = "happy";
    showScreen(SCR_EYES);
  }
  lastWebPoll = millis();

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
  j += ",\"ip\":\""; j += (netMode == NET_AP ? WiFi.softAPIP() : WiFi.localIP()).toString(); j += '"';
  j += ",\"host\":\"" HOSTNAME "\"";
  j += ",\"hotspotOnly\":"; j += HOTSPOT_ONLY ? "true" : "false";
  j += ",\"apSsid\":"; jsonStr(j, HOTSPOT_SSID);
  j += ",\"clients\":"; j += netMode == NET_AP ? (int)WiFi.softAPgetStationNum() : 0;
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
    "on the Info screen. If it can't connect, the " HOTSPOT_SSID " hotspot comes back."
    #if HOTSPOT_ONLY
    " (HOTSPOT_ONLY is on in config.h, so it stays on its own hotspot until that's set to false.)"
    #endif
    "</p>");
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

// ---------- Face: shapes ----------
// The face is built from a few shapes: filled ovals (open eyes), oval
// outlines cut down to a few rows (closed eyes, smiles, frowns, brows),
// hearts and small props. Positions are floats, rounded at the pixel.
#define FRAME_MS   20   // pause between frames; drawing + sending one takes ~25 ms more
#define FACE_CX    64   // face center
#define FACE_CY    32
#define EYE_DX     24   // eye centers: this far left/right of the face center...
#define EYE_DY     -7   // ...and this far above it
#define EYE_RX     8    // open eye: an oval 17 px wide, 21 px tall
#define EYE_RY     10
#define BLUSH_DX   36   // blush marks, below and outside the eyes
#define BLUSH_DY   8
#define MOUTH_DY   10

const float PI_F = 3.14159265f;
const float TAU_F = 6.2831853f;

int iround(float v) { return (int)floorf(v + 0.5f); }

// a round brush: 1 = one pixel, 2 = 2x2, 3+ = filled circle
void dab(int x, int y, int th) {
  if (th <= 1) display.drawPixel(x, y, SSD1306_WHITE);
  else if (th == 2) display.fillRect(x, y, 2, 2, SSD1306_WHITE);
  else display.fillCircle(x, y, th / 2, SSD1306_WHITE);
}

void thickLine(float x0, float y0, float x1, float y1, int th) {
  float dx = x1 - x0, dy = y1 - y0;
  int n = max(1, (int)ceilf(max(fabsf(dx), fabsf(dy))));
  for (int i = 0; i <= n; i++) dab(iround(x0 + dx * i / n), iround(y0 + dy * i / n), th);
}

// part of an oval's outline; angles in degrees, 0 = right, 90 = down
void thickArc(float cx, float cy, float rx, float ry, float a0, float a1, int th) {
  int n = max(4, (int)(fabsf(a1 - a0) / 360 * TAU_F * max(rx, ry)));
  for (int i = 0; i <= n; i++) {
    float a = (a0 + (a1 - a0) * i / n) * (PI_F / 180);
    dab(iround(cx + rx * cosf(a)), iround(cy + ry * sinf(a)), th);
  }
}

// filled oval; lowerOnly = just the bottom half (the big "D" smile)
void fillOval(float cx, float cy, float rx, float ry, uint16_t c, bool lowerOnly) {
  if (ry < 0.5f) ry = 0.5f;
  for (int y = lowerOnly ? iround(cy) : iround(cy - ry); y <= iround(cy + ry); y++) {
    float v = (y - cy) / (ry + 0.5f);
    if (v <= -1 || v >= 1) continue;
    float hw = rx * sqrtf(1 - v * v);
    int xa = iround(cx - hw), xb = iround(cx + hw);
    display.drawFastHLine(xa, y, xb - xa + 1, c);
  }
}

// an oval outline th px thick, drawn only on rows y0..y1. Cut like that it
// gives the closed eyes, smiles, frowns and brows.
void ring(float cx, float cy, float rx, float ry, int th, int y0, int y1) {
  for (int y = y0; y <= y1; y++) {
    float v = (y - cy) / (ry + 0.5f);
    if (v <= -1 || v >= 1) continue;
    float ho = rx * sqrtf(1 - v * v);
    int xa = iround(cx - ho), xb = iround(cx + ho);
    float vi = (ry - th >= 0) ? (y - cy) / (ry - th + 0.5f) : 2;
    if (vi > -1 && vi < 1) {   // this row crosses the hole: a piece on each side
      float hi = (rx - th) * sqrtf(1 - vi * vi);
      int ia = iround(cx - hi), ib = iround(cx + hi);
      display.drawFastHLine(xa, y, max(1, ia - xa), SSD1306_WHITE);
      display.drawFastHLine(min(xb, ib + 1), y, max(1, xb - ib), SSD1306_WHITE);
    } else {
      display.drawFastHLine(xa, y, xb - xa + 1, SSD1306_WHITE);
    }
  }
}

void arcUp(float cx, float cy, float rx, float ry, int th) {     // top half: "^", frown, brow
  ring(cx, cy, rx, ry, th, iround(cy - ry), iround(cy));
}

void arcDown(float cx, float cy, float rx, float ry, int th) {   // bottom half: "u", smile
  ring(cx, cy, rx, ry, th, iround(cy), iround(cy + ry));
}

void heart(float cx, float cy, float R) {
  int r = iround(R);
  display.fillCircle(iround(cx - R * 0.95f), iround(cy - R * 0.35f), r, SSD1306_WHITE);
  display.fillCircle(iround(cx + R * 0.95f), iround(cy - R * 0.35f), r, SSD1306_WHITE);
  display.fillTriangle(iround(cx - R * 1.85f), iround(cy + R * 0.05f), iround(cx + R * 1.85f), iround(cy + R * 0.05f),
                       iround(cx), iround(cy + R * 1.85f), SSD1306_WHITE);
}

void sparkle(int x, int y, int s) {
  if (s <= 0) return;
  display.drawFastVLine(x, y - s, 2 * s + 1, SSD1306_WHITE);
  display.drawFastHLine(x - s, y, 2 * s + 1, SSD1306_WHITE);
  if (s >= 3) display.fillRect(x - 1, y - 1, 3, 3, SSD1306_WHITE);
}

void drop(int x, int y, int r) {   // tear / sweat drop, point up
  display.fillCircle(x, y, r, SSD1306_WHITE);
  display.fillTriangle(x - r, y, x + r, y, x, y - r * 2 - 1, SSD1306_WHITE);
}

void spiral(float cx, float cy, float r, float spin) {   // two turns, middle to rim
  for (int i = 0; i <= 90; i++) {
    float k = i / 90.0f, a = spin + k * 2 * TAU_F;
    dab(iround(cx + r * k * cosf(a)), iround(cy + r * k * sinf(a)), 2);
  }
}

// the cartoon anger mark: four curved pieces around a point
void angerMark(int cx, int cy, int s) {
  for (int i = 0; i < 4; i++) {
    int sx = (i & 1) ? 1 : -1, sy = (i & 2) ? 1 : -1;
    float a = atan2f(-sy, -sx) * (180 / PI_F);
    thickArc(cx + sx * (s + 2), cy + sy * (s + 2), s, s, a - 45, a + 45, 2);
  }
}

void cloud(int cx, int cy, unsigned long t) {
  display.fillCircle(cx - 7, cy + 1, 3, SSD1306_WHITE);
  display.fillCircle(cx - 1, cy - 2, 5, SSD1306_WHITE);
  display.fillCircle(cx + 6, cy, 4, SSD1306_WHITE);
  display.fillRect(cx - 7, cy + 1, 14, 4, SSD1306_WHITE);
  for (int i = 0; i < 3; i++)   // drizzle
    display.drawFastVLine(cx - 6 + i * 6, cy + 7 + (t / 30 + i * 5) % 14, 3, SSD1306_WHITE);
}

void wavy(float cx, float cy, int w, float amp, float phase, int th) {
  float px = 0, py = 0;
  for (int i = 0; i <= w; i++) {
    float x = cx - w / 2.0f + i, y = cy + amp * sinf(i * TAU_F / 8 + phase);
    if (i) thickLine(px, py, x, y, th);
    px = x;
    py = y;
  }
}

void faceText(int x, int y, const char* s, uint8_t size) {
  display.setTextSize(size);
  display.setCursor(x, y);
  display.print(s);
  display.setTextSize(1);
}

// 0 -> 1 -> 0 over w ms, starting at `at`
float bump(unsigned long c, unsigned long at, unsigned long w) {
  float k = 1 - fabsf((float)c - at - w / 2.0f) / (w / 2.0f);
  return k > 0 ? k : 0;
}

// ---------- Face: parts ----------
// open eye; `open` 0..1 squashes it for blinks
void eyeOpen(float x, float y, float rx, float ry, float open) {
  float h = ry * open;
  if (h < 1.5f) display.fillRoundRect(iround(x - rx), iround(y) - 1, iround(2 * rx) + 1, 3, 1, SSD1306_WHITE);   // shut
  else fillOval(x, y, rx, h, SSD1306_WHITE, false);
}

void eyeHappy(float x, float y) { arcUp(x, y + 4, EYE_RX + 1, 8, 3); }     // ^
void eyeSleep(float x, float y) { arcDown(x, y - 2, EYE_RX + 1, 5, 3); }   // u

// Cuts a slanted lid into an open eye: the cut starts at the top at highX
// and runs down to mid-eye at lowX. A brow follows the cut, browGap px above.
void lidCut(float x, float y, float rx, float ry, float highX, float lowX, int browGap) {
  float top = y - ry - 2;
  display.fillTriangle(iround(highX), iround(top), iround(lowX), iround(top), iround(lowX), iround(y - 1), SSD1306_BLACK);
  if (browGap) {
    float k = (y - 1 - top) / (lowX - highX);
    int dir = lowX > highX ? 1 : -1;
    float xa = x - (rx - 1) * dir, xb = x + (rx + 1) * dir;
    thickLine(xa, top + (xa - highX) * k - browGap, xb, top + (xb - highX) * k - browGap, 3);
  }
}

// inner = +1 when the middle of the face is to the eye's right (the left eye)
void lidAngry(float x, float y, float rx, float ry, int inner) {   // "\ /"
  lidCut(x, y, rx, ry, x - inner * (rx + 2), x + inner * (rx + 2), 4);
}

void lidSad(float x, float y, float rx, float ry, int inner) {     // "/ \"
  lidCut(x, y, rx, ry, x + inner * (rx + 2), x - inner * (rx + 2), 4);
}

// heavy eyelids: only the bottom `keep` (0..1) of the eye shows
void lidFlat(float x, float y, float rx, float ry, float keep) {
  int lid = iround(y + ry - 2 * ry * keep), top = iround(y - ry) - 2;
  if (lid > top) display.fillRect(iround(x - rx) - 1, top, iround(2 * rx) + 3, lid - top, SSD1306_BLACK);
}

void blush(float x, float y) { display.fillRoundRect(iround(x) - 5, iround(y) - 1, 11, 3, 1, SSD1306_WHITE); }
void smile(float x, float y) { ring(x, y - 4, 7, 6, 2, iround(y) - 2, iround(y) + 2); }
void frown(float x, float y) { ring(x, y + 4, 7, 6, 2, iround(y) - 2, iround(y) + 2); }
void grin(float x, float y, float rx, float ry) { fillOval(x, y, rx, ry, SSD1306_WHITE, true); }   // "D"

void mouthO(float x, float y, float rx, float ry) {
  fillOval(x, y, rx, ry, SSD1306_WHITE, false);
  if (rx > 2.5f && ry > 2.5f) fillOval(x, y, rx - 2, ry - 2, SSD1306_BLACK, false);
}

void teeth(int x, int y) {   // gritted
  display.fillRoundRect(x - 9, y - 3, 19, 7, 2, SSD1306_WHITE);
  display.drawFastHLine(x - 8, y, 17, SSD1306_BLACK);
  for (int dx = -4; dx <= 4; dx += 4) display.drawFastVLine(x + dx, y - 3, 7, SSD1306_BLACK);
}

// ---------- Face: behaviour shared by the faces ----------
unsigned long faceT0 = 0;                 // when the current face started
unsigned long blinkAt = 0, nextBlinkAt = 0;
float lookX = 0, lookY = 0, lookToX = 0, lookToY = 0;   // gaze, px
unsigned long nextLookAt = 0;

void faceStart(unsigned long now) {
  faceT0 = now;
  blinkAt = now - 1000;
  nextBlinkAt = now + random(800, 2500);
  lookX = lookY = lookToX = lookToY = 0;
  nextLookAt = now + 1500;
}

// 1 = eyes open; dips to 0 and back over 160 ms every few seconds (now and then twice)
float blinkOpen(unsigned long now) {
  if ((long)(now - nextBlinkAt) >= 0) {
    blinkAt = now;
    nextBlinkAt = now + (random(6) == 0 ? 260 : random(2200, 5200));
  }
  unsigned long d = now - blinkAt;
  return d >= 160 ? 1.0f : fabsf((float)d - 80) / 80;
}

void glide(float tx, float ty) {   // ease the gaze toward a spot
  lookX += (tx - lookX) * 0.3f;
  lookY += (ty - lookY) * 0.3f;
}

// the gaze wanders to a new spot every couple of seconds
void wander(unsigned long now, int rx, int ry) {
  if ((long)(now - nextLookAt) >= 0) {
    nextLookAt = now + random(1200, 3500);
    bool center = random(3) == 0;
    lookToX = center ? 0 : random(-rx, rx + 1);
    lookToY = center ? 0 : random(-ry, ry + 1);
  }
  glide(lookToX, lookToY);
}

// ---------- Faces ----------
// Each draws one frame; t = ms since the face started.
void faceNormal(unsigned long now, unsigned long t) {
  wander(now, 9, 4);
  float op = blinkOpen(now);
  float fx = FACE_CX + lookX * 0.4f;
  float fy = FACE_CY + iround(sinf((t % 3000) * TAU_F / 3000)) + lookY * 0.4f;   // breathing
  bool joy = t % 15000 >= 10000 && t % 15000 < 11600;   // a content little smile now and then
  for (int side = -1; side <= 1; side += 2) {
    float ex = fx + side * EYE_DX + lookX * 0.6f, ey = fy + EYE_DY + lookY * 0.6f;
    if (joy) eyeHappy(ex, ey);
    else eyeOpen(ex, ey, EYE_RX, EYE_RY * (1 + side * lookX * 0.012f), op);   // the eye it looks toward grows a bit
    blush(fx + side * BLUSH_DX, fy + BLUSH_DY);
  }
  if (joy) grin(fx, fy + MOUTH_DY - 3, 6, 5);
  else smile(fx, fy + MOUTH_DY);
}

void faceHappy(unsigned long t) {
  unsigned long c = t % 1800;
  float hop = c < 900 ? fabsf(sinf(PI_F * c / 300)) * 3 : 0;   // three little hops, then a rest
  int fx = FACE_CX, fy = FACE_CY - iround(hop);
  for (int side = -1; side <= 1; side += 2) {
    eyeHappy(fx + side * EYE_DX, fy + EYE_DY);
    blush(fx + side * BLUSH_DX, fy + BLUSH_DY);
  }
  grin(fx, fy + MOUTH_DY - 3, 7, hop > 1.5f ? 7 : 6);
  // the little balloon on a curly string, like the picture
  int bx = fx + 44, by = 10 + iround(sinf((t % 1400) * TAU_F / 1400) * 1.5f) - iround(hop);
  float sx = fx + EYE_DX + 9, sy = fy + EYE_DY + 1, px = sx, py = sy;
  for (int i = 1; i <= 10; i++) {
    float k = i / 10.0f, m = 1 - k;
    float qx = m * m * sx + 2 * m * k * (bx - 3) + k * k * (bx - 3);
    float qy = m * m * sy + 2 * m * k * (sy + 1) + k * k * (by + 4);
    thickLine(px, py, qx, qy, 2);
    px = qx;
    py = qy;
  }
  display.fillCircle(bx, by, 5, SSD1306_WHITE);
  // sparkles twinkle on the other side
  const int8_t SPARKS[3][2] = { {14, 14}, {22, 50}, {8, 34} };
  for (int i = 0; i < 3; i++) {
    float k = ((t + i * 600) % 1800) / 600.0f;
    if (k < 1) sparkle(SPARKS[i][0], SPARKS[i][1], iround(3 * sinf(PI_F * k)));
  }
}

void faceSnoring(unsigned long t) {
  const unsigned long P = 3600;   // one breath
  unsigned long c = t % P;
  float p = (float)c / P;
  float s = (1 - cosf(TAU_F * p)) / 2;   // 0 -> 1 -> 0: breathing in, then out
  int fx = FACE_CX, fy = FACE_CY - iround(s * 2) + 1;
  for (int side = -1; side <= 1; side += 2) {
    eyeSleep(fx + side * EYE_DX, fy + EYE_DY + 2);
    blush(fx + side * BLUSH_DX, fy + BLUSH_DY);
  }
  mouthO(fx, fy + MOUTH_DY, 2 + s * 2, 2 + s * 3);
  // nose bubble: swells while breathing out, and every third one pops
  float out = -sinf(TAU_F * p);
  int bx = fx + 5, by = fy + 3;
  if ((t / P) % 3 == 2 && p >= 0.70f) {
    if (p < 0.76f)
      for (int a = 0; a < 360; a += 45) {
        float cs = cosf(a * PI_F / 180), sn = sinf(a * PI_F / 180);
        display.drawLine(iround(bx + 5 + 3 * cs), iround(by + 3 * sn), iround(bx + 5 + 6 * cs), iround(by + 6 * sn), SSD1306_WHITE);
      }
  } else if (out > 0) {
    int r = iround(1 + 4 * out);
    display.drawCircle(bx + r, by, r, SSD1306_WHITE);
    if (r >= 3) display.drawPixel(bx + r + 1, by - r + 2, SSD1306_WHITE);   // shine
  }
  // z Z Z: one after the other, drifting up, then gone
  int rise = iround((float)c / P * 3);
  if (c >= 300 && c < 3300) faceText(97, 17 - rise, "z", 1);
  if (c >= 900 && c < 3300) faceText(105, 9 - rise, "Z", 1);
  if (c >= 1500 && c < 3300) faceText(114, 3 - rise, "Z", 2);
}

void faceAngry(unsigned long now, unsigned long t) {
  bool shaking = t % 2600 < 700;   // fuming, then a cold glare
  int fx = FACE_CX + (shaking ? random(-2, 3) : 0);
  int fy = FACE_CY + 1 + (shaking ? random(-1, 2) : 0);
  float op = blinkOpen(now);
  for (int side = -1; side <= 1; side += 2) {
    int ex = fx + side * EYE_DX, ey = fy + EYE_DY + 1;
    eyeOpen(ex, ey, EYE_RX, EYE_RY, op);
    lidAngry(ex, ey, EYE_RX, EYE_RY, -side);
  }
  if (shaking) teeth(fx, fy + MOUTH_DY + 1);
  else frown(fx, fy + MOUTH_DY + 2);
  angerMark(fx + 47, 13, shaking && (t / 120) % 2 ? 5 : 4);
}

void faceLove(unsigned long t) {
  unsigned long c = t % 1100;
  float beat = bump(c, 0, 160) * 0.25f + bump(c, 260, 160) * 0.15f;   // lub-dub
  float fx = FACE_CX + sinf((t % 2400) * TAU_F / 2400) * 2;
  int fy = FACE_CY;
  for (int side = -1; side <= 1; side += 2) {
    heart(fx + side * EYE_DX, fy + EYE_DY - 2, 4.6f * (1 + beat));
    blush(fx + side * BLUSH_DX, fy + BLUSH_DY + 1);
  }
  grin(fx, fy + MOUTH_DY - 2, 6, 5);
  const int8_t HEART_X[4] = { 7, 117, 15, 109 };   // little hearts floating up the sides
  for (int i = 0; i < 4; i++) {
    float q = ((t + i * 900) % 3600) / 3600.0f;
    float x = HEART_X[i] + 3 * sinf(q * TAU_F * 1.5f + i);
    display.drawBitmap(iround(x) - 3, iround(64 - q * 72), ICON_HEART7, 7, 6, SSD1306_WHITE);
  }
}

void faceAlone(unsigned long now, unsigned long t) {
  unsigned long c = t % 9000;
  // looks down, then hopefully left and right for someone, then down again
  if (c < 4000) glide(0, 4);
  else if (c < 5200) glide(-9, 0);
  else if (c < 6400) glide(9, 0);
  else glide(0, 4);
  float op = blinkOpen(now);
  int fx = FACE_CX, fy = FACE_CY + 1;
  float ry = EYE_RY - 1;
  for (int side = -1; side <= 1; side += 2) {
    float ex = fx + side * EYE_DX + lookX * 0.7f, ey = fy + EYE_DY + lookY * 0.6f;
    eyeOpen(ex, ey, EYE_RX, ry, op);
    lidSad(ex, ey, EYE_RX, ry, -side);
  }
  frown(fx + lookX * 0.3f, fy + MOUTH_DY + 2 + lookY * 0.3f);
  if (c < 3800) {   // a tear wells up under the left eye, then rolls down
    int tx = fx - EYE_DX + 4;
    float y0 = fy + EYE_DY + lookY * 0.6f + ry + 3;
    if (c < 1200) drop(tx, iround(y0), iround(c / 1200.0f * 3));
    else drop(tx, iround(y0 + (c - 1200) / 2600.0f * 40), 3);
  }
  cloud(112, 7, t);
}

void faceConfused(unsigned long now, unsigned long t) {
  int way = t % 5000 < 2500 ? 1 : -1;   // which way the head is cocked
  glide(way * 5, -2);
  float tilt = lookX / 5 * 2;
  float op = blinkOpen(now);
  int fx = FACE_CX, fy = FACE_CY + 1;
  for (int side = -1; side <= 1; side += 2) {
    float ex = fx + side * EYE_DX + lookX * 0.5f;
    float ey = fy + EYE_DY - side * tilt + lookY * 0.5f;
    if (side == way) {   // this eye opens wide under a raised brow...
      eyeOpen(ex, ey, EYE_RX, EYE_RY + 1, op);
      arcUp(ex, ey - EYE_RY - 5, 8, 4, 2);
    } else {             // ...the other one squints
      eyeOpen(ex, ey, EYE_RX, 4, op);
      thickLine(ex - 7, ey - 8 - side * 2, ex + 7, ey - 8 + side * 2, 2);
    }
  }
  wavy(fx + lookX * 0.3f, fy + MOUTH_DY + 2, 14, 1.5f, (t % 1200) * TAU_F / 1200, 2);
  int qx = way > 0 ? 110 : 6;
  faceText(qx, 2 + iround(fabsf(sinf((t % 1000) * PI_F / 500)) * 2), "?", 2);
  if ((t / 400) % 2) faceText(qx + (way > 0 ? -8 : 14), 14, "?", 1);
}

void faceHangover(unsigned long now, unsigned long t) {
  unsigned long c = t % 8000;
  float sway = sinf((t % 2600) * TAU_F / 2600);
  bool dizzy = c >= 4500 && c < 7500;   // the room spins
  bool hic = c >= 2400 && c < 3200;
  float fx = FACE_CX + sway * (dizzy ? 5 : 3);
  int fy = FACE_CY - (c >= 2400 && c < 2550 ? 3 : 0);   // the hiccup jolts it up
  float op = blinkOpen(now);
  float spin = (t % 1000) * TAU_F / 1000;
  for (int side = -1; side <= 1; side += 2) {
    float ex = fx + side * EYE_DX, ey = fy + EYE_DY + side * sway * 1.5f;
    if (dizzy) {
      spiral(ex, ey, 9, side * spin);
    } else {
      eyeOpen(ex, ey, EYE_RX, EYE_RY, op);
      if (!hic) lidFlat(ex, ey, EYE_RX, EYE_RY, side < 0 ? 0.45f : 0.6f);
    }
    blush(fx + side * BLUSH_DX, fy + BLUSH_DY);
  }
  wavy(fx, fy + MOUTH_DY + 2, 16, 1.5f, spin, 2);   // queasy
  if (hic) faceText(iround(fx) + 36, 4, "hic!", 1);
}

void faceSurprised(unsigned long t) {
  float pop = min(1.0f, t / 120.0f);   // eyes pop wide open
  unsigned long c = t % 4000;
  float op = 1 - max(bump(c, 450, 140), bump(c, 700, 140));   // a double blink
  for (int side = -1; side <= 1; side += 2) {
    int ex = FACE_CX + side * EYE_DX, ey = FACE_CY + EYE_DY;
    eyeOpen(ex, ey, EYE_RX + 3 * pop, EYE_RY + pop, op);
    arcUp(ex, ey - 15, 8, 4, 2);
  }
  mouthO(FACE_CX, FACE_CY + MOUTH_DY + 2, 4, 5);
  if ((t / 300) % 2 == 0) faceText(114, 2, "!", 2);
}

void drawFace(int id, unsigned long now) {
  unsigned long t = now - faceT0;
  display.clearDisplay();
  switch (id) {
    case F_HAPPY:     faceHappy(t); break;
    case F_SNORING:   faceSnoring(t); break;
    case F_ANGRY:     faceAngry(now, t); break;
    case F_LOVE:      faceLove(t); break;
    case F_ALONE:     faceAlone(now, t); break;
    case F_CONFUSED:  faceConfused(now, t); break;
    case F_HANGOVER:  faceHangover(now, t); break;
    case F_SURPRISED: faceSurprised(t); break;
    default:          faceNormal(now, t); break;
  }
  display.display();
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
    if (netMode == NET_AP) snprintf(date, sizeof(date), "Join %s", HOTSPOT_SSID);
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

// the "you've got something" reaction: the surprised face pops its eyes
// wide open and blinks twice
void alertEyes() {
  unsigned long t0 = millis();
  while (millis() - t0 < 1300) {
    display.clearDisplay();
    faceSurprised(millis() - t0);
    display.display();
    if (!stayInMode(FRAME_MS)) return;
  }
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
    printClipped(0, 0, HOTSPOT_ONLY ? "Own WiFi hotspot:" : "No home WiFi. Setup:", 21);
    snprintf(line, sizeof(line), "WiFi %s", HOTSPOT_SSID);
    printClipped(0, 11, line, 21);
    snprintf(line, sizeof(line), "Pass %s", HOTSPOT_PASS[0] ? HOTSPOT_PASS : "(none)");
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

// ---------- QR codes ----------
// Dark modules on a lit square, like a printed code, so phone cameras read
// it reliably. 2 px per module when it fits (up to 29x29 modules in 64 px).
int qrX, qrY, qrBox;   // where drawQrModules() puts the next code

void drawQrModules(esp_qrcode_handle_t q) {
  int n = esp_qrcode_get_size(q);
  int scale = (n * 2 + 4 <= qrBox) ? 2 : 1;
  int x0 = qrX + (qrBox - n * scale) / 2, y0 = qrY + (qrBox - n * scale) / 2;
  display.fillRect(qrX, qrY, qrBox, qrBox, SSD1306_WHITE);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(q, x, y))
        display.fillRect(x0 + x * scale, y0 + y * scale, scale, scale, SSD1306_BLACK);
}

void drawQr(int x, int y, int box, const char* text) {
  qrX = x;
  qrY = y;
  qrBox = box;
  esp_qrcode_config_t cfg = { drawQrModules, 4, ESP_QRCODE_ECC_LOW };
  esp_qrcode_generate(&cfg, text);   // too long to fit: nothing drawn, the text still is
}

void appendQrEscaped(String& s, const char* v) {
  for (; *v; v++) {
    if (strchr("\\;,:\"", *v)) s += '\\';
    s += *v;
  }
}

// "WIFI:T:WPA;S:name;P:password;;" - phone cameras offer to join the network
String wifiQrText() {
  String s = HOTSPOT_PASS[0] ? "WIFI:T:WPA;S:" : "WIFI:T:nopass;S:";
  appendQrEscaped(s, HOTSPOT_SSID);
  if (HOTSPOT_PASS[0]) {
    s += ";P:";
    appendQrEscaped(s, HOTSPOT_PASS);
  }
  s += ";;";
  return s;
}

// Hotspot: the "join WiFi" code until a phone is on it, then mostly the
// "open the page" code, with the join code now and then for the next person.
// Home WiFi: only the page code makes sense.
bool connectShowsUrl() {
  if (netMode != NET_AP) return true;
  if (WiFi.softAPgetStationNum() == 0) return false;
  return (millis() - screenSince) % 10000 < 6000;
}

void drawConnect(bool url) {
  const int x = 68;   // text column right of the 64 px code: 10 characters
  display.clearDisplay();
  display.setTextSize(1);
  if (url) {
    char ip[16];
    strlcpy(ip, (netMode == NET_AP ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str(), sizeof(ip));
    char link[32];
    snprintf(link, sizeof(link), "http://%s/", ip);
    drawQr(0, 0, 64, link);
    display.setCursor(x, 0);
    display.print("Scan to");
    display.setCursor(x, 9);
    display.print("open the");
    display.setCursor(x, 18);
    display.print("remote");
    // the address over two lines: "192.168." + "4.1"
    const char* cut = strchr(strchr(ip, '.') + 1, '.') + 1;
    display.setCursor(x, 33);
    display.write((const uint8_t*)ip, cut - ip);
    display.setCursor(x, 42);
    display.print(cut);
  } else {
    drawQr(0, 0, 64, wifiQrText().c_str());
    display.setCursor(x, 0);
    display.print("Scan to");
    display.setCursor(x, 9);
    display.print("join WiFi");
    printClipped(x, 24, HOTSPOT_SSID, 10);
    printClipped(x, 33, HOTSPOT_PASS[0] ? HOTSPOT_PASS : "(open)", 10);
  }
  if (netMode == NET_AP) {
    display.setCursor(x, 56);
    display.print(url ? "step 2/2" : "step 1/2");
  }
  display.display();
}

void runConnect() {
  bool url = connectShowsUrl();
  drawConnect(url);
  // redraw only when it's time to switch codes: building a QR code isn't free
  while (connectShowsUrl() == url)
    if (!stayInMode(250)) return;
}

// branding: a little face over the name (or your own bitmap, see icons.h)
void drawBrandLogo(int dx, int dy, bool eyesClosed) {
  if (CUSTOM_LOGO_W > 0) {
    display.drawBitmap((SCREEN_WIDTH - CUSTOM_LOGO_W) / 2 + dx, (SCREEN_HEIGHT - CUSTOM_LOGO_H) / 2 + dy,
                       CUSTOM_LOGO, CUSTOM_LOGO_W, CUSTOM_LOGO_H, SSD1306_WHITE);
    return;
  }
  int cx = SCREEN_WIDTH / 2 + dx;
  for (int side = -1; side <= 1; side += 2) {
    int x = cx + side * 14, y = 16 + dy;
    if (eyesClosed) arcDown(x, y - 1, 6, 3, 2);
    else fillOval(x, y, 5, 6, SSD1306_WHITE, false);
    display.fillRoundRect(cx + side * 23 - 3, 24 + dy, 7, 2, 1, SSD1306_WHITE);   // blush
  }
  ring(cx, 20 + dy, 4, 4, 2, 22 + dy, 24 + dy);   // smile
  printCenteredFont(&FreeSansBold9pt7b, cx, 50 + dy, BRAND_NAME);
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

// ---------- Face screen ----------
int faceId = -1;                 // face being played, an index into FACES
unsigned long faceScreenAt = 0;  // screenSince it was started for

int faceIndex(const String& m) {
  for (int i = 0; i < FACE_COUNT; i++)
    if (m == FACES[i]) return i;
  return F_NORMAL;
}

// one frame per call; the face starts over when it changes or the eyes
// screen is shown again
void runFace() {
  int id = faceIndex(currentMode);
  if (id != faceId || screenSince != faceScreenAt) {
    faceId = id;
    faceScreenAt = screenSince;
    faceStart(millis());
  }
  drawFace(faceId, millis());
  stayInMode(FRAME_MS);
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

  display.clearDisplay();
  display.display();
  randomSeed(esp_random());

  setenv("TZ", TZ_INFO, 1);   // local time works even before (or without) internet time
  tzset();
  phoneEvents = xQueueCreate(4, sizeof(PhoneEvent));

  loadWifiCredentials();
  if (HOTSPOT_ONLY || !wifiSsid.length()) startHotspot();
  else startStation();

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/screen", handleScreen);
  server.on("/stayon", handleStayOn);
  server.on("/notify", handleNotify);
  server.on("/call", handleCall);
  server.on("/phone", handlePhone);
  server.on("/status", handleStatus);
  server.on("/fb", handleFrame);
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
    case SCR_CONNECT: runConnect(); break;
    case SCR_CALL:    runCall(); break;
    default:          runIdle(); break;
  }
}
