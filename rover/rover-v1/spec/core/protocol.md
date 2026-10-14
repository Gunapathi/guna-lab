# Bluetooth protocol: the contract between phone and rover

> **Owns:** the service and characteristic UUIDs, every packet the phone sends, the checksum, the
> test vectors, every line the rover sends, size limits, the protocol version.
> **Does not own:** what the rover does with a packet ([drive.md](../features/drive.md)), when the
> link connects and when lines are sent ([link.md](../features/link.md)).

---

**Change rule:** to change a packet or a line, edit this file first, bump the protocol version,
then change the firmware and the app together. The version lives in
`firmware/include/config.h` (`PROTOCOL_VERSION`) and, once it exists, the app.

**Version:** 1 (2026-10-14, firmware 0.1.0): B and J packets, the HELLO and OK lines, the echo.

## Service and characteristics

Transport: the **Nordic UART Service (NUS)**, which Bluefruit Connect and nRF Connect understand.

| Name | UUID | Direction | Type |
|---|---|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` | — | — |
| RX | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` | phone → rover | Write, or write without response |
| TX | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` | rover → phone | Notify |

- **No security:** no pairing, no encryption (Bluefruit Connect doesn't pair). Pairing is a later
  item ([roadmap.md](../status/roadmap.md)).
- **One write = one packet** is what senders should do. The parser also copes with a packet
  split across writes, but a write that starts with anything other than `!`, while no packet is
  half read, is text ([below](#text-writes)).
- **Size:** the rover keeps the first 64 bytes of a write (`BLE_WRITE_MAX`); a packet is at most
  19.

## Packets (phone → rover, RX)

Every packet is `!`, a type byte, the payload, then a checksum: the **bitwise NOT of the byte sum
of everything before it, low 8 bits**.

```cpp
uint8_t checksum(const uint8_t* p, size_t n) {
  uint8_t s = 0;
  for (size_t i = 0; i < n; i++) s += p[i];
  return (uint8_t)~s;
}
```

| Type | Bytes | Layout | Sent by |
|---|---|---|---|
| Button | 5 | `!` `B` `<button '1'..'8'>` `<'1' pressed / '0' released>` `<crc>` | Bluefruit Control Pad |
| Joystick | 5 | `!` `J` `<left int8 −100..100>` `<right int8 −100..100>` `<crc>` | The own app (M4) |

- **Buttons:** `1`–`4` are the numbered buttons, `5` Up, `6` Down, `7` Left, `8` Right. The pad
  sends one packet on press and one on release, nothing while held.
- **Joystick:** the values are the final track speeds in % (the app does the mixing,
  [app.md](../features/app.md)). Values outside −100..100 are clamped. The rover expects them at
  least every 300 ms while driving (`JOY_TIMEOUT_MS`); the app sends every 50 ms.
- **Other Bluefruit Controller packets** are read to their end and ignored: `C` colour (6 bytes),
  `A` accelerometer, `G` gyro, `M` magnetometer, `L` location (15 bytes each), `Q` quaternion
  (19 bytes). Skipping them by length means a `!` inside their float payload can't fake a packet.

### Parsing

Byte by byte: wait for `!`, read the type and look up its length, read the rest, check the
checksum, then hand on a B or J packet. A bad checksum or an unknown type drops what was read and
waits for the next `!` (a second `!` right after the first starts again from there). A
disconnect also drops a half-read packet.

### Text writes

A write that starts with anything other than `!` (with no packet half read) is text, for
example typed in Bluefruit's UART screen. The rover sends it straight back on TX: the link
check. Text that starts with `!` is read as packets and dropped.

### Worked examples

Use these as the unit-test vectors (firmware: `test/test_core/test_main.cpp`; the app's tests
when it exists).

| Meaning | Bytes (hex) |
|---|---|
| Up pressed `!B51` | `21 42 35 31 36` |
| Up released `!B50` | `21 42 35 30 37` |
| Joystick L = +60, R = +60 | `21 4A 3C 3C 1C` |
| Joystick L = −60, R = +40 (turn left) | `21 4A C4 28 A8` |
| Joystick stop L = 0, R = 0 | `21 4A 00 00 94` |

## Rover → phone (TX)

Plain ASCII lines ending in `\n`, as notifications. A line longer than the link allows (MTU − 3
bytes, 20 with the default MTU) is split across notifications, so a reader joins the pieces and
splits on `\n`.

| Line | Example | When |
|---|---|---|
| Hello | `HELLO RoverC3 fw=0.1.0 pv=1` | Each time the phone subscribes to TX: the name, firmware version and protocol version |
| Status | `OK spd=2 L=70 R=70` | On every change (at most every 200 ms, `STATUS_MIN_MS`) and every second anyway (`STATUS_MS`) while connected |
| Echo | whatever text the phone wrote | Right after a [text write](#text-writes) |

- **Status fields:** `spd` is the speed level, 1–3 (the button that set it: 40, 70, 100 % by
  default). `L` and `R` are the **target** track speeds in % (−100..100), what was asked for; the
  motors reach them within `RAMP_MS`.
- A reader should ignore lines it doesn't know, and fields it doesn't know inside a known line:
  that is how later versions add to them.
