# Testing by hand with Bluefruit Connect and nRF Connect

> **Owns:** the step-by-step for testing the rover from a phone before the own app exists: the
> UART echo and the Control Pad (Bluefruit Connect), joystick packets and the timeout (nRF
> Connect).
> **Does not own:** the checklists to tick ([testing.md](../status/testing.md)), what each packet
> means ([protocol.md](../core/protocol.md)).

---

**Before you start:** tracks lifted; the serial monitor open if the ESP32 is on USB (then W5
pulled, [hardware.md](../core/hardware.md#bench-rules)). Install **Bluefruit Connect** (Adafruit)
and **nRF Connect for Mobile** (Nordic) on the phone.

## Bluefruit Connect: link check

1. Open Bluefruit Connect. **RoverC3** is in the list (turn on "Must have UART service" to hide
   other devices). Tap **Connect**. Serial: `Bluetooth connected`.
2. Open **UART**. You see `HELLO RoverC3 fw=0.1.0 pv=1`, then `OK spd=1 L=0 R=0` every second.
3. Type `hello` and send: `hello` comes back. The link works both ways.

## Bluefruit Connect: Control Pad

Back → **Controller** → **Control Pad**.

| Do | Expect |
|---|---|
| Hold ▲ | Both tracks forward at 40 %, ramping up over about 100 ms; serial heartbeat `target 40/40` |
| Release ▲ | Both stop (ramped) |
| Hold ◀ / ▶ | Spin in place, left / right |
| Hold ▲ and ◀ | Curve left: the left track at half speed |
| Tap **3**, hold ▲ | 100 %; the status says `spd=3` |
| Hold ▲, tap **4** | Stops, though ▲ is still held |
| Hold ▲, turn off the phone's Bluetooth | Stops within about 100 ms; serial `Bluetooth disconnected: stopped` |
| Turn Bluetooth back on, reconnect | Connects again; nothing moves until a button |

## nRF Connect: joystick packets and the timeout

nRF Connect can write raw bytes, so it can send the J packets the own app will send. One write
is one packet, and the rover drives for about 300 ms before the failsafe stops it. That is the
test of the timeout.

1. **Scanner** → RoverC3 → **Connect**. Open the **Nordic UART Service**.
2. On **TX** (`6E400003…`), tap the subscribe icon: the HELLO and OK lines arrive (show them as
   text).
3. On **RX** (`6E400002…`), tap the write icon, choose **Byte array**, write type **Command**
   (write without response), and send:

| Bytes | Expect |
|---|---|
| `214A3C3C1C` (L = R = +60) | Both tracks forward for about 0.3 s, then a stop; serial `Failsafe: no joystick packet in time, stopped` |
| `214AC428A8` (L = −60, R = +40) | A short turn left, then the failsafe stop |
| `214A000094` (stop) | Nothing moves; the status shows `L=0 R=0` |
| `214A3C3C1D` (bad checksum) | Nothing: the packet is dropped |
| `2142353136`, then `2142353037` | Up pressed, then released: pad mode, no timeout between the two |

4. Disconnect while the rover is still: serial `Bluetooth disconnected: stopped`, and it shows up
   in the scanner again.
