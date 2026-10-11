#ifndef switches_h
#define switches_h

#include "Config.h"

// debounced state of all foot and external switches
class Switches
{
	public:
		void Init();
		// returns true if the switch has changed, pressed holds the new state
		bool Update(uint8_t index, bool & pressed);

	private:
		bool _pressed[NUM_SWITCHES];
		uint32_t _lastChange[NUM_SWITCHES];
};

#endif
