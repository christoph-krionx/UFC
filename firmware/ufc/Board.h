/*
	Board definition for UFC rev. 0 (hardware/UFC) with a RobotDyn Mega2560 Pro.
	Pin mapping taken from the schematic netlist.
*/

#ifndef board_h
#define board_h

#include <Arduino.h>

// lcd FDCC2002C (HD44780, 4 bit mode, r/w tied to gnd)
#define PIN_LCD_RS	5
#define PIN_LCD_EN	7
#define PIN_LCD_DB4	9
#define PIN_LCD_DB5	11
#define PIN_LCD_DB6	13
#define PIN_LCD_DB7	15
#define LCD_COLS	20
#define LCD_ROWS	2

// led power on / off switch (active low)
#define PIN_LED_VCC_ON	12

// WS2812 configuration, two leds per foot switch
#define PIN_LED_DATA	6
#define NUM_LEDS	42
#define LED_COLOR_ORDER	RGB
#define NO_LED	0xFF

// expression pedals, AREF is driven externally (VDDA) -> analogReference(EXTERNAL) is mandatory
#define PIN_EXP_1	A0
#define PIN_EXP_2	A1

// midi 1: J25 in / J23 out, midi 2: J26 in / J24 out
// the loopback test showed out 1 -> in 2 and out 2 -> in 1, check the rx assignment before using the inputs
#define MIDI_PORT_1	Serial1
#define MIDI_PORT_2	Serial2
#define MIDI_BAUDRATE	31250

#define NUM_FOOT_SWITCHES	21
#define NUM_EXT_SWITCHES	4
#define NUM_SWITCHES	(NUM_FOOT_SWITCHES + NUM_EXT_SWITCHES)

struct SwitchDef
{
	uint8_t pin;
	uint8_t ledLeft;
	uint8_t ledRight;
};

// all switches are active low (10k pullup + 100nF on the pcb)
// foot switches are numbered from bottom left to top right,
// the led chain starts at switch 19 (top left) and snakes down
static const SwitchDef SWITCHES[NUM_SWITCHES] =
{
	// row 1 (bottom), switch 1 - 6
	{ 53, 41, 40 }, { 46, 39, 38 }, { 43, 37, 36 }, { 38, 35, 34 }, { 36, 33, 32 }, { 34, 31, 30 },
	// row 2, switch 7 - 12
	{ 39, 18, 19 }, { 52, 20, 21 }, { 50, 22, 23 }, { 48, 24, 25 }, { 47, 26, 27 }, { 42, 28, 29 },
	// row 3, switch 13 - 18
	{ 40, 17, 16 }, { 37, 15, 14 }, { 35, 13, 12 }, { 33, 11, 10 }, { 41, 9, 8 }, { 44, 7, 6 },
	// row 4 (top), switch 19 - 21
	{ 51, 0, 1 }, { 49, 2, 3 }, { 45, 4, 5 },
	// external switches, J29 pin 2 / 3, J30 pin 2 / 3
	{ 21, NO_LED, NO_LED }, { 22, NO_LED, NO_LED }, { 23, NO_LED, NO_LED }, { 24, NO_LED, NO_LED }
};

#endif
