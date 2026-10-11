#include "MidiOut.h"

void MidiOut::Init()
{
	MIDI_PORT_1.begin(MIDI_BAUDRATE);
	MIDI_PORT_2.begin(MIDI_BAUDRATE);
}

void MidiOut::SendProgramChange(uint8_t portMask, uint8_t channel, uint8_t program)
{
	uint8_t data[2] = { (uint8_t)(0xC0 | (channel & 0x0F)), (uint8_t)(program & 0x7F) };
	Write(portMask, data, sizeof(data));
}

void MidiOut::SendControlChange(uint8_t portMask, uint8_t channel, uint8_t controller, uint8_t value)
{
	uint8_t data[3] = { (uint8_t)(0xB0 | (channel & 0x0F)), (uint8_t)(controller & 0x7F), (uint8_t)(value & 0x7F) };
	Write(portMask, data, sizeof(data));
}

void MidiOut::Write(uint8_t portMask, const uint8_t * data, uint8_t size)
{
	if (portMask & PORT_1)
		MIDI_PORT_1.write(data, size);

	if (portMask & PORT_2)
		MIDI_PORT_2.write(data, size);
}
