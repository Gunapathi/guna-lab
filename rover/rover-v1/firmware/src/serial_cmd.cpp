#include "serial_cmd.h"

#include <Arduino.h>

#include "drive.h"

static void printHelp() {
  Serial.println("Bench commands: f forward, b back, l spin left, r spin right, s stop,");
  Serial.println("                1 / 2 / 3 speed level, h this help. Commands latch until s.");
}

void serialPoll(Drive& drive) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    if (c == 'h' || c == '?') printHelp();
    else if (drive.bench(c)) Serial.printf("> %c  spd %d  target L %d R %d\n", c, drive.level() + 1, drive.targetLeft(), drive.targetRight());
    else Serial.printf("? '%c' is not a command (h for help)\n", c);
  }
}
