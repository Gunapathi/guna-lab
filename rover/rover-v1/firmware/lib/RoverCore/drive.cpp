#include "drive.h"

static const PadState NOTHING_HELD = {false, false, false, false};

static int clampPct(int v) {
  return v > 100 ? 100 : v < -100 ? -100 : v;
}

Drive::Drive(const DriveConfig& cfg) : cfg_(cfg), pad_(NOTHING_HELD), level_(0) {
  setLevel(cfg.startLevel);
}

void Drive::packet(const Packet& p, uint32_t now) {
  if (p.type == 'B') button(p.data[0], p.data[1] == '1');
  else if (p.type == 'J') joystick((int8_t)p.data[0], (int8_t)p.data[1], now);
}

// Bluefruit Control Pad: '1'-'3' speed, '4' stop, '5'-'8' up, down, left, right. No timeout in
// this mode: the pad only sends on press and release, so a timeout would stop a held button.
void Drive::button(uint8_t code, bool pressed) {
  switch (code) {
    case '1':
    case '2':
    case '3':
      if (pressed) setLevel(code - '1');
      break;
    case '4':
      if (pressed) pad_ = NOTHING_HELD;
      break;
    case '5': pad_.up = pressed; break;
    case '6': pad_.down = pressed; break;
    case '7': pad_.left = pressed; break;
    case '8': pad_.right = pressed; break;
    default: return;
  }
  joystickMode_ = false;
  aimPad();
}

// The app mixes; its values are the track speeds
void Drive::joystick(int left, int right, uint32_t now) {
  if (!joystickMode_) {
    joystickMode_ = true;
    pad_ = NOTHING_HELD;   // so going back to the pad never resumes a stale direction
  }
  tgtL_ = clampPct(left);
  tgtR_ = clampPct(right);
  lastJoy_ = now;
  joyLive_ = true;
}

// Serial bench commands latch, like the pad's buttons, until 's'
bool Drive::bench(char cmd) {
  PadState p = NOTHING_HELD;
  switch (cmd) {
    case 'f': p.up = true; break;
    case 'b': p.down = true; break;
    case 'l': p.left = true; break;
    case 'r': p.right = true; break;
    case 's': break;
    case '1':
    case '2':
    case '3':
      setLevel(cmd - '1');
      p = pad_;   // same direction, new speed
      break;
    default: return false;
  }
  joystickMode_ = false;
  pad_ = p;
  aimPad();
  return true;
}

void Drive::linkLost() {
  joystickMode_ = false;
  joyLive_ = false;
  pad_ = NOTHING_HELD;
  stopNow();
}

bool Drive::tick(uint32_t now) {
  bool timedOut = joystickMode_ && joyLive_ && now - lastJoy_ > cfg_.joyTimeoutMs;
  if (timedOut) {
    joyLive_ = false;
    stopNow();   // a failsafe coasts at once; stopping draws no current, so the ramp isn't needed
  }
  outL_ = slew(outL_, tgtL_, cfg_.slewStep);
  outR_ = slew(outR_, tgtR_, cfg_.slewStep);
  return timedOut;
}

void Drive::setLevel(int level) {
  level_ = level < 0 ? 0 : level >= cfg_.levels ? cfg_.levels - 1 : level;
}

void Drive::aimPad() {
  padToTracks(pad_, cfg_.speedPct[level_], tgtL_, tgtR_);
}

void Drive::stopNow() {
  tgtL_ = tgtR_ = outL_ = outR_ = 0;
}
