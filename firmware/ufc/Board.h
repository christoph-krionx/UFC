/*
	Board definition for the UFC board (hardware/UFC) with a RobotDyn Mega2560 Pro.
	Pin mapping taken from the schematic netlist.

	The same board is used in several builds, select the variant with -DBOARD=<n>
	(arduino-cli: --build-property "build.extra_flags=-DBOARD=2") or by changing the default below.
*/

#ifndef board_h
#define board_h

#include <Arduino.h>

#define BOARD_UFC21	1	// 21 foot switches with two leds each, 4 external switches, lcd
#define BOARD_STOMP6	2	// 6 foot switches with one led each, lcd

#ifndef BOARD
#define BOARD	BOARD_UFC21
#endif

// led power on / off switch (active low)
#define PIN_LED_VCC_ON	12

// WS2812 chain
#define PIN_LED_DATA	6
#define LED_COLOR_ORDER	RGB
#define NO_LED	0xFF

// lcd FDCC2002C / FC2002C (20x2, HD44780, 4 bit mode, r/w tied to gnd), only used if the board has HAS_DISPLAY
#define PIN_LCD_RS	5
#define PIN_LCD_EN	7
#define PIN_LCD_DB4	9
#define PIN_LCD_DB5	11
#define PIN_LCD_DB6	13
#define PIN_LCD_DB7	15
#define LCD_COLS	20
#define LCD_ROWS	2

// expression pedals, AREF is driven externally (VDDA) -> analogReference(EXTERNAL) is mandatory
#define PIN_EXP_1	A0
#define PIN_EXP_2	A1

// midi 1: J25 in / J23 out, midi 2: J26 in / J24 out
// the loopback test showed out 1 -> in 2 and out 2 -> in 1, check the rx assignment before using the inputs
#define MIDI_PORT_1	Serial1
#define MIDI_PORT_2	Serial2
#define MIDI_BAUDRATE	31250

struct SwitchDef
{
	uint8_t pin;
	uint8_t ledLeft;
	uint8_t ledRight;	// NO_LED: the switch has a single led
};

#if BOARD == BOARD_UFC21
#include "Board_UFC21.h"
#elif BOARD == BOARD_STOMP6
#include "Board_Stomp6.h"
#else
#error "unknown board variant"
#endif

#define NUM_SWITCHES	(NUM_FOOT_SWITCHES + NUM_EXT_SWITCHES)

#endif
