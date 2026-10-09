#pragma once
// Icons for the OLED, in Adafruit GFX drawBitmap() format.
// The ASCII art above each array is the picture ('#' = lit pixel).


// 8x8 - WiFi connected
//   ........
//   .######.
//   #......#
//   ..####..
//   .#....#.
//   ...##...
//   ...##...
//   ........
const uint8_t ICON_WIFI8[] PROGMEM = {
  0x00, 0x7E,
  0x81, 0x3C,
  0x42, 0x18,
  0x18, 0x00,
};

// 8x8 - Bluetooth / phone linked
//   ...#....
//   ...##...
//   .#.#.#..
//   ..###...
//   ..###...
//   .#.#.#..
//   ...##...
//   ...#....
const uint8_t ICON_BT8[] PROGMEM = {
  0x10, 0x18,
  0x54, 0x38,
  0x38, 0x54,
  0x18, 0x10,
};

// 8x8 - unread notifications
//   ...##...
//   ..####..
//   .######.
//   .######.
//   .######.
//   ########
//   ........
//   ...##...
const uint8_t ICON_BELL8[] PROGMEM = {
  0x18, 0x3C,
  0x7E, 0x7E,
  0x7E, 0xFF,
  0x00, 0x18,
};

// 7x6 - little hearts floating up on the "love" face
//   .##.##.
//   #######
//   #######
//   .#####.
//   ..###..
//   ...#...
const uint8_t ICON_HEART7[] PROGMEM = {
  0x6C, 0xFE, 0xFE, 0x7C, 0x38, 0x10,
};

// 16x16 - chat apps: SMS, WhatsApp, Telegram, ...
//   ................
//   ..############..
//   .##############.
//   .##############.
//   .##############.
//   .##############.
//   .##..##..##..##.
//   .##..##..##..##.
//   .##############.
//   .##############.
//   ..############..
//   ..####..........
//   ..###...........
//   ..##............
//   ..#.............
//   ................
const uint8_t ICON_CHAT16[] PROGMEM = {
  0x00, 0x00, 0x3F, 0xFC, 0x7F, 0xFE, 0x7F, 0xFE,
  0x7F, 0xFE, 0x7F, 0xFE, 0x66, 0x66, 0x66, 0x66,
  0x7F, 0xFE, 0x7F, 0xFE, 0x3F, 0xFC, 0x3C, 0x00,
  0x38, 0x00, 0x30, 0x00, 0x20, 0x00, 0x00, 0x00,
};

// 16x16 - e-mail: Gmail, Outlook, ...
//   ................
//   ................
//   ################
//   ##............##
//   #.#..........#.#
//   #..#........#..#
//   #...#......#...#
//   #....#....#....#
//   #.....#..#.....#
//   #......##......#
//   #..............#
//   #..............#
//   ################
//   ................
//   ................
//   ................
const uint8_t ICON_MAIL16[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xC0, 0x03,
  0xA0, 0x05, 0x90, 0x09, 0x88, 0x11, 0x84, 0x21,
  0x82, 0x41, 0x81, 0x81, 0x80, 0x01, 0x80, 0x01,
  0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// 16x16 - phone calls
//   ................
//   ..##............
//   .####...........
//   .#####..........
//   .#####..........
//   ..###...........
//   ..###...........
//   ...###..........
//   ....###.........
//   .....###....##..
//   ......####.####.
//   .......########.
//   ........#######.
//   ..........####..
//   ................
//   ................
const uint8_t ICON_CALL16[] PROGMEM = {
  0x00, 0x00, 0x30, 0x00, 0x78, 0x00, 0x7C, 0x00,
  0x7C, 0x00, 0x38, 0x00, 0x38, 0x00, 0x1C, 0x00,
  0x0E, 0x00, 0x07, 0x0C, 0x03, 0xDE, 0x01, 0xFE,
  0x00, 0xFE, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x00,
};

// 16x16 - everything else
//   .......##.......
//   ......####......
//   ....########....
//   ...##########...
//   ...##########...
//   ..############..
//   ..############..
//   ..############..
//   ..############..
//   .##############.
//   ################
//   ################
//   ................
//   ......####......
//   .......##.......
//   ................
const uint8_t ICON_BELL16[] PROGMEM = {
  0x01, 0x80, 0x03, 0xC0, 0x0F, 0xF0, 0x1F, 0xF8,
  0x1F, 0xF8, 0x3F, 0xFC, 0x3F, 0xFC, 0x3F, 0xFC,
  0x3F, 0xFC, 0x7F, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x03, 0xC0, 0x01, 0x80, 0x00, 0x00,
};

// ---------- Branding logo ----------
// By default the dim screen draws two small eyes + BRAND_NAME (config.h).
// To use your own logo instead: convert a black & white image (max 128x48)
// at https://javl.github.io/image2cpp/ (Code output: "Arduino code",
// Draw mode: "Horizontal - 1 bit per pixel"), paste the bytes below, and
// set CUSTOM_LOGO_W / CUSTOM_LOGO_H to the image size.
#define CUSTOM_LOGO_W 0
#define CUSTOM_LOGO_H 0
const uint8_t CUSTOM_LOGO[] PROGMEM = { 0x00 };

