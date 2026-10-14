#pragma once
// USB serial bench commands (spec/features/serial.md)

class Drive;

void serialPoll(Drive& drive);   // reads every waiting character; call from loop()
