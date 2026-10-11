/*
	ufc_hwtest - hardware test for the UFC rev. 0 board

	Runs all tests once after reset, shows the result on the lcd and
	mirrors a log on the usb serial port (115200 baud).
	Sending 's' on the serial port skips the currently running guided test.
	After the summary any switch enters the monitor mode.
*/

#include <LiquidCrystal.h>
#include <FastLED.h>
#include <EEPROM.h>
#include "Board.h"

#define VERSION "0.1.0"

// tests which need something connected can be disabled here, they are reported as skipped
#define TEST_MIDI_LOOPBACK	1	// din cable from out to in on both ports
#define TEST_FOOT_SWITCHES	1
#define TEST_EXT_SWITCHES	0
#define TEST_EXPRESSION		0

#define LED_TEST_BRIGHTNESS	40
#define LED_MAX_MILLIAMPS	250	// keeps the board usable on usb power
#define SWITCH_DEBOUNCE_MS	20
#define SWITCH_TIMEOUT_MS	15000
#define MIDI_TIMEOUT_MS		100
#define EXP_TIMEOUT_MS		15000
#define EXP_MIN_SPAN		300
#define MIN_FREE_RAM		1024
#define EEPROM_TEST_ADDRESS	(E2END)

LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_DB4, PIN_LCD_DB5, PIN_LCD_DB6, PIN_LCD_DB7);
CRGB leds[NUM_LEDS];

const char * const testNames[NUM_TESTS] = { "System", "Switch idle", "MIDI loop", "Foot switches", "Ext switches", "Expression" };
const uint8_t midiPattern[] = { 0xF0, 0x7D, 0x55, 0x2A, 0xF7 };

TestResult results[NUM_TESTS];
char details[NUM_TESTS][LCD_COLS + 1];
char line[LCD_COLS + 1];
bool monitorMode = false;

//
// helpers
//

void lcdLine(uint8_t row, const char * text)
{
	char buf[LCD_COLS + 1];
	snprintf(buf, sizeof(buf), "%-20s", text);
	lcd.setCursor(0, row);
	lcd.print(buf);
}

void setResult(TestId id, TestResult result)
{
	static const char * const tags[] = { "PASS", "FAIL", "SKIP" };

	results[id] = result;

	Serial.print('[');
	Serial.print(tags[result]);
	Serial.print(F("] "));
	Serial.print(testNames[id]);
	Serial.print(F(": "));
	Serial.println(details[id]);

	lcdLine(0, testNames[id]);
	snprintf(line, sizeof(line), "%s %s", tags[result], details[id]);
	lcdLine(1, line);
	delay(1000);
}

bool skipRequested()
{
	while (Serial.available())
	{
		char c = Serial.read();
		if (c == 's' || c == 'S')
			return true;
	}
	return false;
}

void switchName(uint8_t index, char * name, uint8_t size)
{
	if (index < NUM_FOOT_SWITCHES)
		snprintf(name, size, "SW %u", index + 1);
	else
		snprintf(name, size, "EXT %u", index - NUM_FOOT_SWITCHES + 1);
}

void setSwitchLeds(uint8_t index, CRGB color)
{
	if (SWITCHES[index].firstLed == NO_LED)
		return;

	for (uint8_t i = 0; i < LEDS_PER_SWITCH; i++)
		leds[SWITCHES[index].firstLed + i] = color;

	FastLED.show();
}

// returns the index of the first pressed switch or -1
int8_t pressedSwitch()
{
	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		if (digitalRead(SWITCHES[i].pin) == LOW)
		{
			delay(SWITCH_DEBOUNCE_MS);
			if (digitalRead(SWITCHES[i].pin) == LOW)
				return i;
		}
	}
	return -1;
}

int freeRam()
{
	extern int __heap_start, *__brkval;
	int v;
	return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

//
// visual checks, not part of the result
//

void showLcdPattern()
{
	lcdLine(0, "UFC hwtest " VERSION);
	lcdLine(1, "UFC rev. 0");
	delay(1500);

	// all pixels on
	for (uint8_t row = 0; row < LCD_ROWS; row++)
	{
		lcd.setCursor(0, row);
		for (uint8_t col = 0; col < LCD_COLS; col++)
			lcd.write(0xFF);
	}
	delay(1000);

	lcdLine(0, "0123456789ABCDEFGHIJ");
	lcdLine(1, "abcdefghijklmnopqrst");
	delay(1000);
}

void showLedPattern()
{
	static const CRGB colors[] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::White };
	static const char * const names[] = { "RED", "GREEN", "BLUE", "WHITE" };

	for (uint8_t c = 0; c < 4; c++)
	{
		snprintf(line, sizeof(line), "all leds %s", names[c]);
		lcdLine(0, "LED test");
		lcdLine(1, line);
		fill_solid(leds, NUM_LEDS, colors[c]);
		FastLED.show();
		delay(800);
	}

	// chase in switch order to check the led mapping
	fill_solid(leds, NUM_LEDS, CRGB::Black);
	lcdLine(1, "chase SW 1 -> SW 21");
	for (uint8_t i = 0; i < NUM_FOOT_SWITCHES; i++)
	{
		setSwitchLeds(i, CRGB::White);
		delay(120);
		setSwitchLeds(i, CRGB::Black);
	}
}

//
// tests
//

TestResult testSystem()
{
	int ram = freeRam();

	// write the inverted value to the last eeprom cell and restore it afterwards
	uint8_t original = EEPROM.read(EEPROM_TEST_ADDRESS);
	EEPROM.write(EEPROM_TEST_ADDRESS, ~original);
	bool eepromOk = (EEPROM.read(EEPROM_TEST_ADDRESS) == (uint8_t)~original);
	EEPROM.write(EEPROM_TEST_ADDRESS, original);
	eepromOk = eepromOk && (EEPROM.read(EEPROM_TEST_ADDRESS) == original);

	snprintf(details[TEST_ID_SYSTEM], sizeof(details[0]), "RAM %d EE %s", ram, eepromOk ? "ok" : "bad");

	return (ram >= MIN_FREE_RAM && eepromOk) ? RESULT_PASS : RESULT_FAIL;
}

// no switch may be active without being pressed (stuck switch or short to gnd)
TestResult testSwitchIdle()
{
	uint8_t stuck = 0;
	char name[8];

	details[TEST_ID_SWITCH_IDLE][0] = 0;

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		if (digitalRead(SWITCHES[i].pin) == LOW)
		{
			switchName(i, name, sizeof(name));
			Serial.print(F("  stuck low: "));
			Serial.println(name);

			if (stuck == 0)
				snprintf(details[TEST_ID_SWITCH_IDLE], sizeof(details[0]), "%s stuck", name);
			stuck++;
		}
	}

	if (stuck > 1)
		snprintf(details[TEST_ID_SWITCH_IDLE], sizeof(details[0]), "%u stuck", stuck);
	else if (stuck == 0)
		strcpy(details[TEST_ID_SWITCH_IDLE], "all released");

	return stuck ? RESULT_FAIL : RESULT_PASS;
}

// sends the test pattern and returns which inputs received it (bit 0 = in 1, bit 1 = in 2)
uint8_t midiSendAndListen(HardwareSerial & out)
{
	HardwareSerial * inputs[2] = { &MIDI_PORT_1, &MIDI_PORT_2 };
	uint8_t pos[2] = { 0, 0 };

	for (uint8_t k = 0; k < 2; k++)
		while (inputs[k]->available())
			inputs[k]->read();

	out.write(midiPattern, sizeof(midiPattern));
	out.flush();

	uint32_t start = millis();
	while (millis() - start < MIDI_TIMEOUT_MS)
	{
		for (uint8_t k = 0; k < 2; k++)
		{
			while (inputs[k]->available())
			{
				uint8_t b = inputs[k]->read();

				if (pos[k] >= sizeof(midiPattern))
					continue;

				if (b == midiPattern[pos[k]])
					pos[k]++;
				else
					pos[k] = (b == midiPattern[0]) ? 1 : 0;
			}
		}
	}

	return (pos[0] == sizeof(midiPattern) ? 1 : 0) | (pos[1] == sizeof(midiPattern) ? 2 : 0);
}

TestResult testMidiLoopback()
{
	static const char * const targets[] = { "--", "I1", "I2", "I1+2" };

	uint8_t from1 = midiSendAndListen(MIDI_PORT_1);
	uint8_t from2 = midiSendAndListen(MIDI_PORT_2);

	snprintf(details[TEST_ID_MIDI], sizeof(details[0]), "O1>%s O2>%s", targets[from1], targets[from2]);

	// straight or crossed cabling is fine as long as every out and every in is covered
	return (from1 && from2 && (from1 | from2) == 3) ? RESULT_PASS : RESULT_FAIL;
}

// asks for every switch in order, the leds of the switch show blue (waiting), green (ok) or red (timeout)
TestResult testSwitchWalk(TestId id, uint8_t first, uint8_t count)
{
	uint8_t failed = 0;
	char name[8];
	char wrongName[8];

	for (uint8_t i = first; i < first + count; i++)
	{
		switchName(i, name, sizeof(name));
		snprintf(line, sizeof(line), "Press %s", name);
		lcdLine(0, line);
		lcdLine(1, "");
		setSwitchLeds(i, CRGB::Blue);

		bool ok = false;
		int8_t lastWrong = -1;
		uint32_t start = millis();

		while (!ok && millis() - start < SWITCH_TIMEOUT_MS)
		{
			if (skipRequested())
			{
				setSwitchLeds(i, CRGB::Black);
				strcpy(details[id], "skipped by user");
				return RESULT_SKIP;
			}

			int8_t pressed = pressedSwitch();

			if (pressed == i)
			{
				ok = true;
			}
			else if (pressed >= 0 && pressed != lastWrong)
			{
				// another input reacted, most likely swapped connectors
				lastWrong = pressed;
				switchName(pressed, wrongName, sizeof(wrongName));
				snprintf(line, sizeof(line), "got %s instead", wrongName);
				lcdLine(1, line);
				Serial.print(F("  expected "));
				Serial.print(name);
				Serial.print(F(", got "));
				Serial.println(wrongName);
			}
		}

		if (ok)
		{
			setSwitchLeds(i, CRGB::Green);

			// wait for release
			start = millis();
			while (digitalRead(SWITCHES[i].pin) == LOW && millis() - start < SWITCH_TIMEOUT_MS)
				;
			delay(SWITCH_DEBOUNCE_MS);
		}
		else
		{
			setSwitchLeds(i, CRGB::Red);
			Serial.print(F("  timeout: "));
			Serial.println(name);

			if (failed == 0)
				snprintf(details[id], sizeof(details[0]), "%s timeout", name);
			failed++;
		}
	}

	if (failed > 1)
		snprintf(details[id], sizeof(details[0]), "%u timeouts", failed);
	else if (failed == 0)
		snprintf(details[id], sizeof(details[0]), "%u ok", count);

	return failed ? RESULT_FAIL : RESULT_PASS;
}

// both pedals have to be moved over their full range, any switch ends the test early
TestResult testExpression()
{
	const uint8_t pins[2] = { PIN_EXP_1, PIN_EXP_2 };
	int minValue[2] = { 1023, 1023 };
	int maxValue[2] = { 0, 0 };

	lcdLine(0, "Move EXP 1 + 2 fully");

	uint32_t start = millis();
	while (millis() - start < EXP_TIMEOUT_MS)
	{
		if (skipRequested())
		{
			strcpy(details[TEST_ID_EXPRESSION], "skipped by user");
			return RESULT_SKIP;
		}

		for (uint8_t k = 0; k < 2; k++)
		{
			// first conversion after switching the channel is discarded
			analogRead(pins[k]);
			int value = analogRead(pins[k]);
			minValue[k] = min(minValue[k], value);
			maxValue[k] = max(maxValue[k], value);
		}

		snprintf(line, sizeof(line), "1:%4d 2:%4d", analogRead(PIN_EXP_1), analogRead(PIN_EXP_2));
		lcdLine(1, line);

		if (pressedSwitch() >= 0)
			break;

		delay(50);
	}

	int span1 = maxValue[0] - minValue[0];
	int span2 = maxValue[1] - minValue[1];

	Serial.print(F("  exp 1 min/max: "));
	Serial.print(minValue[0]);
	Serial.print('/');
	Serial.println(maxValue[0]);
	Serial.print(F("  exp 2 min/max: "));
	Serial.print(minValue[1]);
	Serial.print('/');
	Serial.println(maxValue[1]);

	snprintf(details[TEST_ID_EXPRESSION], sizeof(details[0]), "span %d/%d", span1, span2);

	return (span1 >= EXP_MIN_SPAN && span2 >= EXP_MIN_SPAN) ? RESULT_PASS : RESULT_FAIL;
}

void skipTest(TestId id)
{
	strcpy(details[id], "disabled");
	setResult(id, RESULT_SKIP);
}

//
// summary and monitor mode
//

void countResults(uint8_t & failed, uint8_t & skipped)
{
	failed = 0;
	skipped = 0;

	for (uint8_t i = 0; i < NUM_TESTS; i++)
	{
		if (results[i] == RESULT_FAIL)
			failed++;
		else if (results[i] == RESULT_SKIP)
			skipped++;
	}
}

void showSummaryHeader()
{
	uint8_t failed, skipped;
	countResults(failed, skipped);

	if (failed)
		snprintf(line, sizeof(line), "FAILED: %u test%s", failed, failed > 1 ? "s" : "");
	else if (skipped)
		snprintf(line, sizeof(line), "PASS (%u skipped)", skipped);
	else
		strcpy(line, "ALL TESTS PASSED");

	lcdLine(0, line);
	Serial.println(line);
}

// cycles through all tests which did not pass, alternating name and detail
void updateSummary()
{
	static uint32_t lastUpdate = 0;
	static uint8_t index = 0;
	static bool showDetail = false;

	if (lastUpdate != 0 && millis() - lastUpdate < 1500)
		return;
	lastUpdate = millis();

	uint8_t failed, skipped;
	countResults(failed, skipped);

	if (failed + skipped == 0)
	{
		lcdLine(1, "any switch: monitor");
		return;
	}

	if (!showDetail)
	{
		// advance to the next test which did not pass
		do
		{
			index = (index + 1) % NUM_TESTS;
		} while (results[index] == RESULT_PASS);

		snprintf(line, sizeof(line), "%s %s", results[index] == RESULT_FAIL ? "FAIL" : "SKIP", testNames[index]);
		lcdLine(1, line);
	}
	else
	{
		lcdLine(1, details[index]);
	}

	showDetail = !showDetail;
}

void monitorMidi(HardwareSerial & port, uint8_t number)
{
	static char midiLine[LCD_COLS + 1];
	static uint8_t lastPort = 0;

	while (port.available())
	{
		uint8_t b = port.read();

		// ignore realtime messages (clock, active sensing)
		if (b >= 0xF8)
			continue;

		// every status byte starts a new line
		if (number != lastPort || (b & 0x80))
		{
			snprintf(midiLine, sizeof(midiLine), "IN%u:", number);
			lastPort = number;
		}

		uint8_t len = strlen(midiLine);
		if (len + 3 <= LCD_COLS)
			snprintf(midiLine + len, sizeof(midiLine) - len, " %02X", b);

		lcdLine(1, midiLine);
	}
}

// shows switch events and incoming midi data
void updateMonitor()
{
	static bool down[NUM_SWITCHES];
	char name[8];

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
	{
		bool now = (digitalRead(SWITCHES[i].pin) == LOW);

		if (now != down[i])
		{
			down[i] = now;
			switchName(i, name, sizeof(name));
			snprintf(line, sizeof(line), "%s %s", name, now ? "down" : "up");
			lcdLine(0, line);
			Serial.println(line);
			setSwitchLeds(i, now ? CRGB::White : CRGB::Black);
			delay(SWITCH_DEBOUNCE_MS);
		}
	}

	monitorMidi(MIDI_PORT_1, 1);
	monitorMidi(MIDI_PORT_2, 2);
}

void setup()
{
	Serial.begin(115200);
	Serial.println(F("UFC hwtest " VERSION));

	// keep the leds unpowered until the data line is defined
	pinMode(PIN_LED_VCC_ON, OUTPUT);
	digitalWrite(PIN_LED_VCC_ON, HIGH);

	// AREF is driven externally, never use the default reference on this board
	analogReference(EXTERNAL);

	for (uint8_t i = 0; i < NUM_SWITCHES; i++)
		pinMode(SWITCHES[i].pin, INPUT);

	MIDI_PORT_1.begin(MIDI_BAUDRATE);
	MIDI_PORT_2.begin(MIDI_BAUDRATE);

	FastLED.addLeds<WS2812B, PIN_LED_DATA, LED_COLOR_ORDER>(leds, NUM_LEDS);
	FastLED.setBrightness(LED_TEST_BRIGHTNESS);
	FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MILLIAMPS);
	fill_solid(leds, NUM_LEDS, CRGB::Black);
	delay(200);
	digitalWrite(PIN_LED_VCC_ON, LOW);
	delay(50);
	FastLED.show();

	lcd.begin(LCD_COLS, LCD_ROWS);
	lcd.clear();

	showLcdPattern();
	showLedPattern();

	setResult(TEST_ID_SYSTEM, testSystem());
	setResult(TEST_ID_SWITCH_IDLE, testSwitchIdle());

#if TEST_MIDI_LOOPBACK
	setResult(TEST_ID_MIDI, testMidiLoopback());
#else
	skipTest(TEST_ID_MIDI);
#endif

#if TEST_FOOT_SWITCHES
	setResult(TEST_ID_FOOT_SWITCHES, testSwitchWalk(TEST_ID_FOOT_SWITCHES, 0, NUM_FOOT_SWITCHES));
#else
	skipTest(TEST_ID_FOOT_SWITCHES);
#endif

#if TEST_EXT_SWITCHES
	setResult(TEST_ID_EXT_SWITCHES, testSwitchWalk(TEST_ID_EXT_SWITCHES, NUM_FOOT_SWITCHES, NUM_EXT_SWITCHES));
#else
	skipTest(TEST_ID_EXT_SWITCHES);
#endif

#if TEST_EXPRESSION
	setResult(TEST_ID_EXPRESSION, testExpression());
#else
	skipTest(TEST_ID_EXPRESSION);
#endif

	fill_solid(leds, NUM_LEDS, CRGB::Black);
	FastLED.show();

	showSummaryHeader();
}

void loop()
{
	if (monitorMode)
	{
		updateMonitor();
		return;
	}

	updateSummary();

	if (pressedSwitch() >= 0)
	{
		monitorMode = true;
		lcdLine(0, "Monitor mode");
		lcdLine(1, "");
	}
}
