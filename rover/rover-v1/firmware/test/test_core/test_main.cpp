// Unit tests for lib/RoverCore, on the PC: pio test -e native
// Vectors from spec/core/protocol.md#worked-examples, tables from spec/features/drive.md.
#include <string.h>
#include <unity.h>

#include "drive.h"
#include "mixer.h"
#include "protocol.h"

void setUp() {}
void tearDown() {}

// ---------- Helpers ----------
static int feedAll(PacketParser& parser, const uint8_t* bytes, size_t n, Packet* got, int max) {
  int count = 0;
  Packet p;
  for (size_t i = 0; i < n; i++)
    if (parser.feed(bytes[i], p) && count < max) got[count++] = p;
  return count;
}

static Packet button(char code, bool pressed) {
  Packet p = {'B', {(uint8_t)code, (uint8_t)(pressed ? '1' : '0')}};
  return p;
}

static Packet stick(int left, int right) {
  Packet p = {'J', {(uint8_t)(int8_t)left, (uint8_t)(int8_t)right}};
  return p;
}

static const int LEVELS[3] = {40, 70, 100};
static const DriveConfig CONFIG = {LEVELS, 3, 0, 4, 300};

static void tickUntilSettled(Drive& d, uint32_t& now) {
  for (int i = 0; i < 60; i++) d.tick(now += 10);
}

// ---------- Protocol ----------
static const uint8_t UP_PRESSED[] = {0x21, 0x42, 0x35, 0x31, 0x36};
static const uint8_t UP_RELEASED[] = {0x21, 0x42, 0x35, 0x30, 0x37};
static const uint8_t JOY_FORWARD[] = {0x21, 0x4A, 0x3C, 0x3C, 0x1C};
static const uint8_t JOY_TURN_LEFT[] = {0x21, 0x4A, 0xC4, 0x28, 0xA8};
static const uint8_t JOY_STOP[] = {0x21, 0x4A, 0x00, 0x00, 0x94};

void test_checksum_vectors() {
  const uint8_t* vectors[] = {UP_PRESSED, UP_RELEASED, JOY_FORWARD, JOY_TURN_LEFT, JOY_STOP};
  for (const uint8_t* v : vectors) TEST_ASSERT_EQUAL_HEX8(v[4], checksum(v, 4));
}

void test_parser_buttons() {
  PacketParser parser;
  Packet got[2];
  TEST_ASSERT_EQUAL(1, feedAll(parser, UP_PRESSED, 5, got, 2));
  TEST_ASSERT_EQUAL_CHAR('B', got[0].type);
  TEST_ASSERT_EQUAL_CHAR('5', got[0].data[0]);
  TEST_ASSERT_EQUAL_CHAR('1', got[0].data[1]);
  TEST_ASSERT_EQUAL(1, feedAll(parser, UP_RELEASED, 5, got, 2));
  TEST_ASSERT_EQUAL_CHAR('0', got[0].data[1]);
}

void test_parser_joystick() {
  PacketParser parser;
  Packet got[1];
  TEST_ASSERT_EQUAL(1, feedAll(parser, JOY_FORWARD, 5, got, 1));
  TEST_ASSERT_EQUAL_CHAR('J', got[0].type);
  TEST_ASSERT_EQUAL(60, (int8_t)got[0].data[0]);
  TEST_ASSERT_EQUAL(60, (int8_t)got[0].data[1]);
  TEST_ASSERT_EQUAL(1, feedAll(parser, JOY_TURN_LEFT, 5, got, 1));
  TEST_ASSERT_EQUAL(-60, (int8_t)got[0].data[0]);
  TEST_ASSERT_EQUAL(40, (int8_t)got[0].data[1]);
  TEST_ASSERT_EQUAL(1, feedAll(parser, JOY_STOP, 5, got, 1));
  TEST_ASSERT_EQUAL(0, (int8_t)got[0].data[0]);
  TEST_ASSERT_EQUAL(0, (int8_t)got[0].data[1]);
}

void test_parser_bad_checksum_is_dropped_then_resyncs() {
  PacketParser parser;
  Packet got[2];
  const uint8_t bad[] = {0x21, 0x42, 0x35, 0x31, 0x37};
  TEST_ASSERT_EQUAL(0, feedAll(parser, bad, 5, got, 2));
  TEST_ASSERT_FALSE(parser.busy());
  TEST_ASSERT_EQUAL(1, feedAll(parser, UP_PRESSED, 5, got, 2));
}

void test_parser_skips_text_and_unknown_types() {
  PacketParser parser;
  Packet got[2];
  const uint8_t stream[] = {'h', 'i', '!', 'Z', 'x', 0x21, 0x21, 0x42, 0x35, 0x31, 0x36};   // "!!" restarts
  TEST_ASSERT_EQUAL(1, feedAll(parser, stream, sizeof stream, got, 2));
  TEST_ASSERT_EQUAL_CHAR('5', got[0].data[0]);
}

void test_parser_reads_other_bluefruit_packets_to_their_end() {
  // a quaternion packet whose payload holds a whole valid "Up pressed": it must not come out
  uint8_t q[19] = {'!', 'Q', 0x21, 0x42, 0x35, 0x31, 0x36, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  q[18] = checksum(q, 18);
  PacketParser parser;
  Packet got[2];
  TEST_ASSERT_EQUAL(0, feedAll(parser, q, sizeof q, got, 2));
  TEST_ASSERT_FALSE(parser.busy());
  TEST_ASSERT_EQUAL(1, feedAll(parser, UP_RELEASED, 5, got, 2));
  TEST_ASSERT_EQUAL_CHAR('0', got[0].data[1]);
}

void test_parser_reset_drops_half_a_packet() {
  PacketParser parser;
  Packet got[2];
  feedAll(parser, UP_PRESSED, 3, got, 2);
  TEST_ASSERT_TRUE(parser.busy());
  parser.reset();
  TEST_ASSERT_EQUAL(1, feedAll(parser, JOY_STOP, 5, got, 2));
}

void test_status_lines() {
  char line[40];
  TEST_ASSERT_EQUAL(20, formatStatus(line, sizeof line, 1, 70, -35));
  TEST_ASSERT_EQUAL_STRING("OK spd=2 L=70 R=-35\n", line);
  TEST_ASSERT_EQUAL(7, formatStatus(line, 8, 0, 0, 0));   // cut short, still terminated
  formatHello(line, sizeof line, "RoverC3", "0.1.0", 1);
  TEST_ASSERT_EQUAL_STRING("HELLO RoverC3 fw=0.1.0 pv=1\n", line);
}

// ---------- Mixer ----------
static void assertPad(bool up, bool down, bool left, bool right, int wantL, int wantR) {
  PadState pad = {up, down, left, right};
  int l = 99, r = 99;
  padToTracks(pad, 70, l, r);
  TEST_ASSERT_EQUAL(wantL, l);
  TEST_ASSERT_EQUAL(wantR, r);
}

void test_pad_table() {
  assertPad(false, false, false, false, 0, 0);
  assertPad(true, true, false, false, 0, 0);
  assertPad(true, false, false, false, 70, 70);
  assertPad(false, true, false, false, -70, -70);
  assertPad(false, false, true, false, -70, 70);
  assertPad(false, false, false, true, 70, -70);
  assertPad(true, false, true, false, 35, 70);
  assertPad(true, false, false, true, 70, 35);
  assertPad(false, true, true, false, -35, -70);
  assertPad(false, true, false, true, -70, -35);
}

void test_pad_opposites_cancel() {
  assertPad(false, false, true, true, 0, 0);
  assertPad(true, true, true, false, -70, 70);   // Up + Down cancel, Left still spins
}

void test_slew() {
  TEST_ASSERT_EQUAL(4, slew(0, 70, 4));
  TEST_ASSERT_EQUAL(70, slew(68, 70, 4));
  TEST_ASSERT_EQUAL(-4, slew(0, -70, 4));
  TEST_ASSERT_EQUAL(46, slew(50, 0, 4));
  TEST_ASSERT_EQUAL(0, slew(2, 0, 4));
}

void test_motor_duty() {
  TEST_ASSERT_EQUAL(0, motorDuty(0, 90, 230));
  TEST_ASSERT_EQUAL(90, motorDuty(1, 90, 230));
  TEST_ASSERT_EQUAL(90, motorDuty(-1, 90, 230));
  TEST_ASSERT_EQUAL(230, motorDuty(100, 90, 230));
  TEST_ASSERT_EQUAL(230, motorDuty(-100, 90, 230));
  TEST_ASSERT_EQUAL(230, motorDuty(150, 90, 230));
  TEST_ASSERT_EQUAL(159, motorDuty(50, 90, 230));   // 90 + 49 * 140 / 99
}

// ---------- Drive ----------
void test_drive_starts_still_at_the_start_level() {
  Drive d(CONFIG);
  TEST_ASSERT_EQUAL(0, d.level());
  TEST_ASSERT_EQUAL(0, d.left());
  TEST_ASSERT_EQUAL(0, d.right());
}

void test_drive_pad_ramps_up_and_down() {
  Drive d(CONFIG);
  uint32_t now = 0;
  d.packet(button('2', true), now);
  d.packet(button('5', true), now);
  TEST_ASSERT_EQUAL(70, d.targetLeft());
  d.tick(now += 10);
  TEST_ASSERT_EQUAL(4, d.left());   // one step, not a jump
  tickUntilSettled(d, now);
  TEST_ASSERT_EQUAL(70, d.left());
  TEST_ASSERT_EQUAL(70, d.right());
  d.packet(button('5', false), now);
  d.tick(now += 10);
  TEST_ASSERT_EQUAL(66, d.left());   // a release ramps down
  tickUntilSettled(d, now);
  TEST_ASSERT_EQUAL(0, d.left());
}

void test_drive_full_ramp_fits_ramp_ms() {
  Drive d(CONFIG);
  uint32_t now = 0;
  d.packet(button('3', true), now);
  d.packet(button('5', true), now);
  for (int i = 0; i < 25; i++) d.tick(now += 10);   // 250 ms at 4 % a tick
  TEST_ASSERT_EQUAL(100, d.left());
}

void test_drive_pad_has_no_timeout() {
  Drive d(CONFIG);
  uint32_t now = 0;
  d.packet(button('5', true), now);
  for (int i = 0; i < 100; i++) TEST_ASSERT_FALSE(d.tick(now += 100));
  TEST_ASSERT_EQUAL(40, d.left());
}

void test_drive_level_change_while_held() {
  Drive d(CONFIG);
  d.packet(button('5', true), 0);
  TEST_ASSERT_EQUAL(40, d.targetLeft());
  d.packet(button('3', true), 0);
  TEST_ASSERT_EQUAL(100, d.targetLeft());
  TEST_ASSERT_EQUAL(2, d.level());
}

void test_drive_stop_button_clears_held_directions() {
  Drive d(CONFIG);
  d.packet(button('5', true), 0);
  d.packet(button('7', true), 0);
  TEST_ASSERT_EQUAL(20, d.targetLeft());
  TEST_ASSERT_EQUAL(40, d.targetRight());
  d.packet(button('4', true), 0);
  TEST_ASSERT_EQUAL(0, d.targetLeft());
  TEST_ASSERT_EQUAL(0, d.targetRight());
  d.packet(button('4', false), 0);
  d.packet(button('5', false), 0);
  TEST_ASSERT_EQUAL(0, d.targetLeft());
}

void test_drive_joystick_timeout_stops_at_once() {
  Drive d(CONFIG);
  uint32_t now = 1000;
  d.packet(stick(60, 60), now);
  tickUntilSettled(d, now);   // 600 ms without packets: already timed out on the way
  TEST_ASSERT_EQUAL(0, d.left());

  now = 5000;
  d.packet(stick(60, 60), now);
  for (int i = 0; i < 20; i++) d.tick(now += 10);   // 200 ms: up to speed, still fed
  TEST_ASSERT_EQUAL(60, d.left());
  d.packet(stick(60, 60), now);                     // the last packet
  TEST_ASSERT_FALSE(d.tick(now + 300));             // exactly the timeout: still going
  TEST_ASSERT_EQUAL(60, d.left());
  TEST_ASSERT_TRUE(d.tick(now + 301));              // past it: stopped, no ramp
  TEST_ASSERT_EQUAL(0, d.left());
  TEST_ASSERT_EQUAL(0, d.right());
  TEST_ASSERT_EQUAL(0, d.targetLeft());
  TEST_ASSERT_FALSE(d.tick(now + 400));             // reported once

  d.packet(stick(30, 30), now + 500);               // packets again: it drives again, ramped
  d.tick(now + 510);
  TEST_ASSERT_EQUAL(4, d.left());
}

void test_drive_link_lost_stops_at_once() {
  Drive d(CONFIG);
  uint32_t now = 0;
  d.packet(button('5', true), now);
  tickUntilSettled(d, now);
  TEST_ASSERT_EQUAL(40, d.left());
  d.linkLost();
  TEST_ASSERT_EQUAL(0, d.left());
  TEST_ASSERT_EQUAL(0, d.right());
  d.tick(now += 10);
  TEST_ASSERT_EQUAL(0, d.left());   // the held Up is forgotten
}

void test_drive_joystick_values_are_clamped() {
  Drive d(CONFIG);
  d.packet(stick(-128, 127), 0);
  TEST_ASSERT_EQUAL(-100, d.targetLeft());
  TEST_ASSERT_EQUAL(100, d.targetRight());
}

void test_drive_joystick_forgets_held_pad_directions() {
  Drive d(CONFIG);
  d.packet(button('5', true), 0);
  d.packet(stick(0, 0), 0);
  d.packet(button('7', true), 0);   // back on the pad: only Left is held now
  TEST_ASSERT_EQUAL(-40, d.targetLeft());
  TEST_ASSERT_EQUAL(40, d.targetRight());
}

void test_drive_bench_commands() {
  Drive d(CONFIG);
  TEST_ASSERT_TRUE(d.bench('f'));
  TEST_ASSERT_EQUAL(40, d.targetLeft());
  TEST_ASSERT_TRUE(d.bench('3'));   // same direction, new speed
  TEST_ASSERT_EQUAL(100, d.targetRight());
  TEST_ASSERT_TRUE(d.bench('l'));
  TEST_ASSERT_EQUAL(-100, d.targetLeft());
  TEST_ASSERT_EQUAL(100, d.targetRight());
  TEST_ASSERT_TRUE(d.bench('s'));
  TEST_ASSERT_EQUAL(0, d.targetLeft());
  TEST_ASSERT_FALSE(d.bench('x'));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_vectors);
  RUN_TEST(test_parser_buttons);
  RUN_TEST(test_parser_joystick);
  RUN_TEST(test_parser_bad_checksum_is_dropped_then_resyncs);
  RUN_TEST(test_parser_skips_text_and_unknown_types);
  RUN_TEST(test_parser_reads_other_bluefruit_packets_to_their_end);
  RUN_TEST(test_parser_reset_drops_half_a_packet);
  RUN_TEST(test_status_lines);
  RUN_TEST(test_pad_table);
  RUN_TEST(test_pad_opposites_cancel);
  RUN_TEST(test_slew);
  RUN_TEST(test_motor_duty);
  RUN_TEST(test_drive_starts_still_at_the_start_level);
  RUN_TEST(test_drive_pad_ramps_up_and_down);
  RUN_TEST(test_drive_full_ramp_fits_ramp_ms);
  RUN_TEST(test_drive_pad_has_no_timeout);
  RUN_TEST(test_drive_level_change_while_held);
  RUN_TEST(test_drive_stop_button_clears_held_directions);
  RUN_TEST(test_drive_joystick_timeout_stops_at_once);
  RUN_TEST(test_drive_link_lost_stops_at_once);
  RUN_TEST(test_drive_joystick_values_are_clamped);
  RUN_TEST(test_drive_joystick_forgets_held_pad_directions);
  RUN_TEST(test_drive_bench_commands);
  return UNITY_END();
}
