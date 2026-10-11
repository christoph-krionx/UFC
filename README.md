# UFC – configurable MIDI foot controller

A MIDI foot controller for guitarists, built around an Arduino Mega 2560 Pro.
Every switch can send program changes and control changes to amps, modellers and effects.
The assignment is edited on the device itself or with a web editor over USB.

## Hardware

Board **UFC rev. 0** (the KiCad project will be added to this repository later):

- RobotDyn Mega 2560 Pro (ATmega2560, CH340 USB serial)
- 21 foot switches in four rows (3 + 6 + 6 + 6), each with two WS2812 LEDs
- 4 inputs for external switches, 2 inputs for expression pedals
- 20x2 character LCD (HD44780)
- 2 MIDI inputs and 2 MIDI outputs (5 pin DIN)

The pin mapping is documented in `src/ufc/Board.h`.

## Repository

| Folder | Content |
| --- | --- |
| `src/ufc` | controller firmware |
| `src/ufc_hwtest` | stand-alone hardware test |
| `editor` | web editor (plain HTML and JavaScript, no build step) |

## Firmware

Features of the current version:

- 8 presets with names, the last used preset is restored after power on
- up to 3 messages per switch (program change or control change), each with MIDI output and channel
- switch modes: toggle, momentary and group (only one switch of the group is active)
- global switches which keep their function in every preset, for example preset up / down or tuner
- messages which are sent when a preset is loaded
- configuration stored in the internal EEPROM, every block protected by a checksum

### Build and upload

The firmware is a plain Arduino sketch. Required libraries: FastLED and LiquidCrystal.

```bash
arduino-cli compile --fqbn arduino:avr:mega src/ufc
```

```bash
arduino-cli upload --fqbn arduino:avr:mega -p <port> src/ufc
```

The Arduino IDE works as well: open `src/ufc/ufc.ino` and select the board "Arduino Mega or Mega 2560".

### Edit mode on the device

Hold switch 19 and 20 (top row) for one second. The bottom row then works as keys:

| Switch | Function |
| --- | --- |
| 1 / 2 | previous / next field |
| 3 / 4 | value down / up, hold to repeat |
| 5 | back, unsaved changes are discarded |
| 6 | save |

Stepping on any other switch selects it for editing.

Holding switch 19 and 21 during power on restores the factory configuration.

## Web editor

The editor talks to the controller with the Web Serial API and therefore needs Chrome or Edge on a desktop computer.

```bash
python -m http.server 8137 --bind 127.0.0.1 --directory editor
```

Open `http://localhost:8137`, connect and select the serial port of the controller.
The configuration can also be saved to and loaded from a file.

The serial protocol is described in `src/ufc/Protocol.h`, the data format in `src/ufc/Config.h` and `src/ufc/Storage.h`.

## Hardware test

`src/ufc_hwtest` checks display, LEDs, switches and both MIDI ports and shows the result on the display.
The MIDI test needs a DIN cable from each output to an input.

## License

MIT, see [LICENSE](LICENSE).
