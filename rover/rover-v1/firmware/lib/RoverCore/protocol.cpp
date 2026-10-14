#include "protocol.h"

#include <stdio.h>
#include <string.h>

uint8_t checksum(const uint8_t* p, size_t n) {
  uint8_t s = 0;
  for (size_t i = 0; i < n; i++) s += p[i];
  return (uint8_t)~s;
}

// Whole length of a Bluefruit packet by its type byte, 0 = not a packet type
static size_t packetLength(uint8_t type) {
  switch (type) {
    case 'B': return 5;    // button: number, state
    case 'J': return 5;    // joystick (this rover's own): left, right
    case 'C': return 6;    // colour picker: r, g, b
    case 'A':              // accelerometer, gyro, magnetometer, location: 3 floats
    case 'G':
    case 'M':
    case 'L': return 15;
    case 'Q': return 19;   // quaternion: 4 floats
    default: return 0;
  }
}

bool PacketParser::feed(uint8_t b, Packet& out) {
  if (len_ == 0) {
    if (b == '!') buf_[len_++] = b;
    return false;
  }
  if (len_ == 1) {
    need_ = packetLength(b);
    if (need_ == 0) {
      len_ = b == '!' ? 1 : 0;   // "!!": the second one may be the real start
      return false;
    }
  }
  buf_[len_++] = b;
  if (len_ < need_) return false;

  len_ = 0;
  if (checksum(buf_, need_ - 1) != buf_[need_ - 1]) return false;
  if (buf_[1] != 'B' && buf_[1] != 'J') return false;
  out.type = (char)buf_[1];
  out.data[0] = buf_[2];
  out.data[1] = buf_[3];
  return true;
}

// snprintf's return value on a cut line differs between C libraries (some give -1 and leave the
// string unterminated), so terminate and measure here instead
static size_t finish(char* out, size_t cap) {
  out[cap - 1] = 0;
  return strlen(out);
}

size_t formatHello(char* out, size_t cap, const char* name, const char* fw, int protocol) {
  snprintf(out, cap, "HELLO %s fw=%s pv=%d\n", name, fw, protocol);
  return finish(out, cap);
}

size_t formatStatus(char* out, size_t cap, int level, int left, int right) {
  snprintf(out, cap, "OK spd=%d L=%d R=%d\n", level + 1, left, right);
  return finish(out, cap);
}
