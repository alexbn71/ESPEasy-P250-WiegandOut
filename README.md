# ESPEasy plugin P250 – Wiegand Output

[ESPEasy](https://github.com/letscontrolit/ESPEasy) plugin that makes an ESP32 act as a **Wiegand
reader / keypad**: it sends codes on two GPIOs (D0, D1) to a device with a Wiegand input, such as
the reader input of an alarm panel or an access controller.

- Any frame length from 4 to 64 bits, with or without the standard parity bits (26 bit, 34 bit, ...)
- Facility code + card number, or a raw value
- Keypad emulation, 4 or 8 bits per key
- Open drain, push-pull or inverted output (to drive a transistor)
- Frames are queued and sent without blocking ESPEasy between keys

Built and tested against ESPEasy `mega-20260720` on ESP32.

## Files

| File | Content |
|---|---|
| `src/_P250_WiegandOut.ino` | the plugin (settings page, commands) |
| `src/src/PluginStructs/P250_data_struct.*` | frame queue and GPIO timing |
| `src/src/PluginStructs/P250_wiegand.h` | frame encoding (parity, keypad), no ESPEasy dependencies |
| `test/test_wiegand.cpp` | host unit tests of the encoding |

## Adding it to an ESPEasy build

1. Copy the `src/` folder of this repository over the `src/` folder of ESPEasy.
2. Enable the plugin in `src/Custom.h` of ESPEasy:
   ```c
   #define USES_P250   // Output - Wiegand (emulator)
   ```
3. Build a custom environment, e.g. `pio run -e custom_ESP32_4M316k`.

ESPEasy already reserves the slot of plugin 250, so no other file needs to be changed.

The [Build](.github/workflows/build.yml) workflow does this for every push and publishes a small test
firmware (Switch, Wiegand reader, SysInfo, Dummy, P250) as the `espeasy-p250-test-firmware`
artifact.

## Settings

*Devices → Add → "Output - Wiegand (emulator)"*

| Setting | Default | |
|---|---|---|
| GPIO D0 / D1 | – | two output GPIOs |
| Output mode | Open drain | see *Wiring* |
| Frame length | 26 bit | 4..64 |
| Add parity bits | on | leading even parity over the first half, trailing odd parity over the second half |
| Key format | 4 bit per key | or 8 bit per key, high nibble = inverted key |
| Pause between keys | 50 ms | also used between queued frames |
| Pulse width / Bit interval | 50 µs / 1000 µs | |

## Commands

Numbers can be decimal, `0x..` (hex) or `0b..` (binary).

| Command | |
|---|---|
| `WiegandOut,<code>` | send `<code>` with the configured frame (adding parity if enabled) |
| `WiegandOut,<facility>,<card>` | code = facility × 65536 + card; for 26 bit: facility 0..255, card 0..65535 |
| `WiegandOutRaw,<bits>,<value>` | send exactly `<bits>` bits of `<value>`, nothing added |
| `WiegandOutKeys,<keys>` | send keypad keys: `0`-`9`, `*` (10), `#` (11), `A`-`F` |

Examples: `WiegandOut,123,4567`, `WiegandOut,0x7B11D7`, `WiegandOutKeys,1234#`.

The task value `Code` holds the last frame sent (parity bits included). When all queued frames are
sent it is sent to the controllers and triggers the rules event `<taskname>#Code`.

Commands reach every P250 task: use a single P250 task per ESP.

## Wiring

The ESP32 runs at 3.3 V and its GPIOs are **not 5 V tolerant**. Wiegand inputs are usually pulled up
to 5 V (or 12 V) by the receiving device.

- Recommended: one NPN transistor (e.g. BC547, base through 1 kΩ) or N-MOSFET (e.g. 2N7000) per
  line, collector/drain on D0 or D1, emitter/source to GND, *Output mode* = **Inverted**.
- Or a bidirectional level shifter (BSS138) with *Output mode* = **Open drain**.
- Connect the ESP32 GND to the GND of the receiving device.

To test without a panel, wire the two P250 GPIOs to the two GPIOs of a *RFID - Wiegand* (P008) task.

## License

Like ESPEasy, this plugin is released under the [GNU General Public License v3.0](LICENSE)
(or, at your option, any later version).
