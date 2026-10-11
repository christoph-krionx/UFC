#ifndef midiout_h
#define midiout_h

#include "Config.h"

// sends channel messages to one or both midi ports
class MidiOut
{
	public:
		void Init();
		void SendProgramChange(uint8_t portMask, uint8_t channel, uint8_t program);
		void SendControlChange(uint8_t portMask, uint8_t channel, uint8_t controller, uint8_t value);

	private:
		void Write(uint8_t portMask, const uint8_t * data, uint8_t size);
};

#endif
