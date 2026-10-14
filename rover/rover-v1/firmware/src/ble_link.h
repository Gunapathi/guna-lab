#pragma once
// The phone link: a NimBLE Nordic UART Service (spec/features/link.md, spec/core/protocol.md).
// NimBLE callbacks only queue bytes and set flags; loop() does the rest.
#include <stddef.h>
#include <stdint.h>

void bleBegin();
bool bleConnected();
size_t bleRead(uint8_t* buf, size_t max);   // the next write from the phone (one write, cut to max); 0 if none
bool bleTakeSubscribed();                   // true once after each time the phone subscribes to TX
void bleSend(const char* line);             // text on TX, split to fit the link's MTU
