#pragma once
// The L298N through LEDC PWM (spec/features/drive.md#motor-output)

void motorsBegin();                          // all four inputs LOW, then PWM on them: first thing in setup()
void motorsSet(int leftPct, int rightPct);   // -100..100 each; 0 coasts
