# Bluetooth link

> **Owns:** advertising, one phone at a time, connect and disconnect, the hello, when the status
> line is sent, the text echo, the write queue.
> **Does not own:** UUIDs and line formats ([protocol.md](../core/protocol.md)), what a
> disconnect does to the motors ([drive.md](drive.md#failsafes)).

---

## Advertising

- The name **RoverC3** (`DEVICE_NAME`) goes in the scan response and the NUS service UUID in the
  advert, so an app can find the rover by either.
- TX power `BLE_TX_DBM` (9 dBm).
- It advertises from power-on until a phone connects, and starts again by itself when the phone
  disconnects.

## One phone at a time

A connection stops the advertising, so a second phone can't see the rover until the first one
lets go. There is no pairing: any phone in range can connect while it advertises. Pairing is a
later item ([roadmap.md](../status/roadmap.md)).

## Connect

1. A phone connects: serial says `Bluetooth connected`. Nothing moves until a packet comes.
2. It subscribes to TX: the rover sends the **HELLO** line, then the status line within
   `STATUS_MIN_MS` (200 ms).

## Status line

The `OK` line ([protocol.md](../core/protocol.md#rover--phone-tx)) goes out while connected:

- **On a change** of the speed level or a target: as soon as `STATUS_MIN_MS` (200 ms) has
  passed since the last line, so the newest value always goes out but a moving joystick can't
  flood the link.
- **Every `STATUS_MS`** (1 s) anyway, as a heartbeat.

## Disconnect

The rover stops at once ([drive.md](drive.md#failsafes)), serial says `Bluetooth disconnected:
stopped`, and it advertises again.

## Text echo

A write that isn't a packet ([protocol.md](../core/protocol.md#text-writes)) is sent straight
back on TX. In Bluefruit Connect's UART screen, whatever you type comes back: the link works
both ways.

## Write queue

NimBLE's task copies each write (its first `BLE_WRITE_MAX`, 64, bytes) into a queue of
`BLE_WRITE_QUEUE` (16) writes, and `loop()` empties it on every pass. A full queue drops new
writes rather than block the Bluetooth task. At 20 joystick packets a second and a loop that
runs every millisecond or so, it doesn't fill.
