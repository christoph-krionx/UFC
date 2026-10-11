# UFC – configurable MIDI foot controller

A MIDI foot controller for guitarists, built around an Arduino Mega 2560 Pro.
Every switch can send program changes and control changes to amps, modellers and effects.
The assignment is edited on the device itself or with a web editor over USB.

## Hardware

Board **UFC rev. 0** (KiCad 9 project in `hardware/UFC`, schematic as [PDF](hardware/UFC/UFC.pdf)):

- RobotDyn Mega 2560 Pro (ATmega2560, CH340 USB serial)
- 21 foot switches in four rows (3 + 6 + 6 + 6), each with two WS2812 LEDs
- 4 inputs for external switches, 2 inputs for expression pedals
- 20x2 character LCD (HD44780)
- 2 MIDI inputs and 2 MIDI outputs (5 pin DIN)

### Pin assignment

The firmware takes the mapping from `firmware/ufc/Board.h`.

| Function | Arduino pin | Connector | Notes |
| --- | --- | --- | --- |
| LCD RS / EN | D5 / D7 | DISP1 | 4 bit mode, R/W tied to GND |
| LCD DB4 – DB7 | D9, D11, D13, D15 | DISP1 | contrast set with trimmer RV1 |
| LED data | D6 | J22 | WS2812 chain, 220 Ω in series |
| LED power enable | D12 | – | active low, switches the LED supply |
| MIDI 1 out / in | D18 (TX1) / D19 (RX1) | J23 / J25 | `Serial1`, input with optocoupler |
| MIDI 2 out / in | D16 (TX2) / D17 (RX2) | J24 / J26 | `Serial2`, input with optocoupler |
| Expression pedal 1 / 2 | A0 / A1 | J27 / J28 | AREF is driven externally, the firmware has to use `analogReference(EXTERNAL)` |
| External switch 1 / 2 | D21 / D22 | J29 | active low |
| External switch 3 / 4 | D23 / D24 | J30 | active low |
| USB serial (editor, upload) | D0 / D1 | USB | CH340 on the Mega 2560 Pro |
| Supply | VIN / 5V | J32 / J31 | |

Unused pins: D2, D3, D4, D8, D10, D14, D20, D25 – D32, A2 – A15.

### Foot switches and LEDs

The switches are numbered from bottom left to top right as seen by the player.
All switch inputs are active low with a 10 kΩ pull-up and 100 nF on the board.
The LED chain starts at switch 19 and snakes down row by row, the table shows the position of the left and right LED in the chain (0 – 41).

| Row | Switch | Arduino pin | Connector | LED left / right |
| --- | --- | --- | --- | --- |
| 4 (top) | 19 | D51 | J1 | 0 / 1 |
| | 20 | D49 | J2 | 2 / 3 |
| | 21 | D45 | J3 | 4 / 5 |
| 3 | 13 | D40 | J9 | 17 / 16 |
| | 14 | D37 | J8 | 15 / 14 |
| | 15 | D35 | J7 | 13 / 12 |
| | 16 | D33 | J6 | 11 / 10 |
| | 17 | D41 | J5 | 9 / 8 |
| | 18 | D44 | J4 | 7 / 6 |
| 2 | 7 | D39 | J10 | 18 / 19 |
| | 8 | D52 | J11 | 20 / 21 |
| | 9 | D50 | J12 | 22 / 23 |
| | 10 | D48 | J13 | 24 / 25 |
| | 11 | D47 | J14 | 26 / 27 |
| | 12 | D42 | J15 | 28 / 29 |
| 1 (bottom) | 1 | D53 | J21 | 41 / 40 |
| | 2 | D46 | J20 | 39 / 38 |
| | 3 | D43 | J19 | 37 / 36 |
| | 4 | D38 | J18 | 35 / 34 |
| | 5 | D36 | J17 | 33 / 32 |
| | 6 | D34 | J16 | 31 / 30 |

## Repository

| Folder | Content |
| --- | --- |
| `firmware/ufc` | controller firmware |
| `firmware/ufc_hwtest` | stand-alone hardware test |
| `editor` | web editor (plain HTML and JavaScript, no build step) |
| `hardware/UFC` | schematic, layout, project libraries and Gerber files |

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
arduino-cli compile --fqbn arduino:avr:mega firmware/ufc
```

```bash
arduino-cli upload --fqbn arduino:avr:mega -p <port> firmware/ufc
```

The Arduino IDE works as well: open `firmware/ufc/ufc.ino` and select the board "Arduino Mega or Mega 2560".

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

The serial protocol is described in `firmware/ufc/Protocol.h`, the data format in `firmware/ufc/Config.h` and `firmware/ufc/Storage.h`.

## Hardware test

`firmware/ufc_hwtest` checks display, LEDs, switches and both MIDI ports and shows the result on the display.
The MIDI test needs a DIN cable from each output to an input.

## License

MIT, see [LICENSE](LICENSE).
