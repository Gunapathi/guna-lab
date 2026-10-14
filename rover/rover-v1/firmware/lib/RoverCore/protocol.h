#pragma once
// The Bluetooth wire format (spec/core/protocol.md). Pure C++, no Arduino headers: tested on
// the PC (test/test_core).
#include <stddef.h>
#include <stdint.h>

// A packet the rover acts on: '!' + type + 2 payload bytes + checksum
struct Packet {
  char type;         // 'B' button, 'J' joystick
  uint8_t data[2];   // B: button '1'..'8', then '1' pressed / '0' released. J: left, right as int8
};

// Bitwise NOT of the byte sum, low 8 bits
uint8_t checksum(const uint8_t* p, size_t n);

// Byte-by-byte state machine: wait for '!', read the type, read that type's payload, check the
// checksum. B and J come out as packets; the other Bluefruit Controller packets (colour,
// sensors) are read to their end and dropped, so a '!' inside their payload can't fake a start.
// A bad checksum or an unknown type resets it to wait for the next '!'.
class PacketParser {
 public:
  bool feed(uint8_t b, Packet& out);   // true when a valid B or J packet just ended
  bool busy() const { return len_ > 0; }
  void reset() { len_ = 0; }

 private:
  static const size_t MAX_LEN = 19;    // the longest Bluefruit packet (quaternion)
  uint8_t buf_[MAX_LEN];
  size_t len_ = 0;    // bytes held, '!' included; 0 = waiting for '!'
  size_t need_ = 0;   // this packet's whole length, checksum included
};

// Text lines on TX, '\n' included. Return the length written.
size_t formatHello(char* out, size_t cap, const char* name, const char* fw, int protocol);
size_t formatStatus(char* out, size_t cap, int level, int left, int right);   // level: index, sent 1-based
