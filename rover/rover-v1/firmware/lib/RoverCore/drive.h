#pragma once
// The rover's control state (spec/features/drive.md): who is driving (the Control Pad, the
// joystick, a serial bench command), the targets, the failsafes and the ramp. Pure C++; time
// comes in as a parameter, so the failsafes are tested on the PC.
#include <stdint.h>

#include "mixer.h"
#include "protocol.h"

struct DriveConfig {
  const int* speedPct;     // speed of each level, % (SPEED_LEVELS_PCT)
  int levels;
  int startLevel;          // index at power-on
  int slewStep;            // most a track's % may change per tick
  uint32_t joyTimeoutMs;   // joystick mode: stop when no J packet came for this long
};

class Drive {
 public:
  explicit Drive(const DriveConfig& cfg);

  void packet(const Packet& p, uint32_t now);   // a B or J packet from the phone
  bool bench(char cmd);                         // serial f b l r s 1 2 3; false if unknown
  void linkLost();                              // Bluetooth dropped: stop at once, no ramp
  bool tick(uint32_t now);                      // every CONTROL_TICK_MS; true when the joystick timeout just stopped it

  int left() const { return outL_; }            // what the motors get now, % (after the ramp)
  int right() const { return outR_; }
  int targetLeft() const { return tgtL_; }      // what was asked for, %
  int targetRight() const { return tgtR_; }
  int level() const { return level_; }          // index into the speed levels

 private:
  void button(uint8_t code, bool pressed);
  void joystick(int left, int right, uint32_t now);
  void setLevel(int level);
  void aimPad();                                // targets from the held directions
  void stopNow();                               // targets and outputs to 0, skipping the ramp

  DriveConfig cfg_;
  PadState pad_;
  bool joystickMode_ = false;
  bool joyLive_ = false;                        // a J packet came and its timeout hasn't fired
  uint32_t lastJoy_ = 0;
  int level_;
  int tgtL_ = 0, tgtR_ = 0, outL_ = 0, outR_ = 0;
};
