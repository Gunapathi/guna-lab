#pragma once
// Control pad -> track speeds, the slew limiter, track speed -> PWM duty
// (spec/features/drive.md). Pure C++; every tunable comes in as a parameter, so tuning
// config.h never breaks a test.

// Directions held on the Control Pad (or set by a serial bench command)
struct PadState {
  bool up, down, left, right;
};

// Track speeds in % (-100..100) for the held directions at speed s (%). Up + Down cancel, and
// so do Left + Right; a turn while driving slows the inner track to s/2.
void padToTracks(const PadState& pad, int s, int& left, int& right);

// current moved toward target by at most maxStep
int slew(int current, int target, int maxStep);

// |pct| 1..100 -> minDuty..maxDuty, 0 -> 0. The sign is the caller's: it picks the pin.
int motorDuty(int pct, int minDuty, int maxDuty);
