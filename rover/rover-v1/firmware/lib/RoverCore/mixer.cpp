#include "mixer.h"

void padToTracks(const PadState& pad, int s, int& left, int& right) {
  int v = (pad.up ? 1 : 0) - (pad.down ? 1 : 0);      // forward / back
  int h = (pad.right ? 1 : 0) - (pad.left ? 1 : 0);   // turn
  if (v == 0) {                                       // still, or spin in place
    left = h * s;
    right = -h * s;
    return;
  }
  int fast = v * s, slow = v * s / 2;
  left = h < 0 ? slow : fast;
  right = h > 0 ? slow : fast;
}

int slew(int current, int target, int maxStep) {
  if (target > current + maxStep) return current + maxStep;
  if (target < current - maxStep) return current - maxStep;
  return target;
}

int motorDuty(int pct, int minDuty, int maxDuty) {
  int a = pct < 0 ? -pct : pct;
  if (a == 0) return 0;
  if (a > 100) a = 100;
  return minDuty + (a - 1) * (maxDuty - minDuty) / 99;
}
