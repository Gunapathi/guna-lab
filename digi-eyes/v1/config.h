#pragma once
// =====================================================================
//  Digi Eyes - your settings. Edit these, then upload.
// =====================================================================

// ---------- Home WiFi ----------
// The device now joins your router like any other gadget, so your phone
// keeps its internet AND can open the control page at the same time.
// These are only the first-boot defaults: you can change the WiFi later
// from the control page ("Change home WiFi") without re-flashing.
// Put your real ones in secrets.h (not committed); these are the fallback.
#if __has_include("secrets.h")
  #include "secrets.h"
#endif
#ifndef HOME_WIFI_SSID
  #define HOME_WIFI_SSID "YOUR_WIFI_NAME"
  #define HOME_WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

// ---------- Fallback setup hotspot ----------
// Only started when the home WiFi can't be reached. Keep this name
// different from your router's, otherwise your phone may join the wrong one.
#define SETUP_AP_SSID    "DigiEyes-Setup"
#define SETUP_AP_PASS    "digieyes123"     // 8+ characters
#define AP_RETRY_MS      (3UL * 60 * 1000) // retry home WiFi this often (router may still be booting)

#define HOSTNAME         "digieyes"        // control page: http://digieyes.local (or the IP on the Info screen)
#define WIFI_CONNECT_TIMEOUT_MS 20000
// Reduced TX power is the brownout fix from before (see FIX NOTES in the .ino).
// If the device is far from the router and drops off, try WIFI_POWER_11dBm.
#define WIFI_TX_POWER    WIFI_POWER_8_5dBm

// ---------- Time ----------
// POSIX time zone string. India = "IST-5:30". Others: https://github.com/nayarsystems/posix_tz_db
#define TZ_INFO          "IST-5:30"
#define NTP_SERVER_1     "pool.ntp.org"
#define NTP_SERVER_2     "time.google.com"
#define CLOCK_24H        false

// ---------- Screen timing ----------
#define EYES_SCREEN_MS     5000   // eyes shown on power-up / wake, then the clock
#define SCREEN_TIMEOUT_MS  5000   // clock, battery and info screens, then dim logo
#define NOTIFY_SCREEN_MS   8000   // a new notification (longer messages get extra time to scroll)
#define CALL_SCREEN_MS     45000  // safety net in case the "call ended" message never arrives

// ---------- Dim / branding screen ----------
#define BRAND_NAME       "DIGI EYES"
#define DIM_CONTRAST     1        // 0-255, brightness of the dimmed logo screen
#define DIM_DEEP         true     // also lower the panel's drive voltage: much dimmer than contrast alone
                                  // (set false if your panel flickers when dimmed)

// ---------- Phone link over Bluetooth (Chronos app, Android) ----------
// Gives you phone notifications, incoming-call alerts, phone battery and
// time sync. Needs Tools -> Partition Scheme -> "Huge APP (3MB No OTA)".
// Set to 0 to build without Bluetooth (fits the default partition).
#define USE_BLE_CHRONOS  1
#define BLE_NAME         "Digi Eyes"       // the name you pick in the Chronos app

// ---------- Device battery (optional) ----------
// The XIAO can't measure its own battery without help. If you run it from
// a LiPo on the BAT pads, wire BAT+ -> 220k -> A0 -> 220k -> GND and set
// this to 2 (A0 is GPIO2). Leave at -1 when powered from USB.
#define BATTERY_ADC_PIN  -1
#define BATTERY_DIVIDER  2.0
