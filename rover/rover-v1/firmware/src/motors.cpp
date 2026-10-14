#include "motors.h"

#include <Arduino.h>

#include "config.h"
#include "mixer.h"

// IN1..IN4; on Arduino core 2.x their index is also their LEDC channel
static const int PINS[4] = {PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};

// ---------- PWM ----------
// Core 2.x drives LEDC by channel, core 3.x by pin; this builds on either
// (spec/core/build.md#arduino-core).
static void pwmAttach(int i) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(PINS[i], PWM_FREQ_HZ, PWM_BITS);
#else
  ledcSetup(i, PWM_FREQ_HZ, PWM_BITS);
  ledcAttachPin(PINS[i], i);
#endif
}

static void pwmWrite(int i, int duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PINS[i], duty);
#else
  ledcWrite(i, duty);
#endif
}

void motorsBegin() {
  for (int pin : PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  for (int i = 0; i < 4; i++) {
    pwmAttach(i);
    pwmWrite(i, 0);
  }
}

// ---------- One motor ----------
// > 0: A = PWM, B = 0. < 0: A = 0, B = PWM. 0: both 0 (coast). The pin going quiet is written
// first, so A and B are never driven together.
static void setMotor(int a, int b, int pct, bool invert) {
  if (invert) pct = -pct;
  int duty = motorDuty(pct, MIN_DUTY, MAX_DUTY);
  if (pct > 0) {
    pwmWrite(b, 0);
    pwmWrite(a, duty);
  } else {
    pwmWrite(a, 0);
    pwmWrite(b, duty);
  }
}

void motorsSet(int leftPct, int rightPct) {
  setMotor(0, 1, leftPct, INVERT_LEFT);
  setMotor(2, 3, rightPct, INVERT_RIGHT);
}
