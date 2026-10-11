#ifndef factory_h
#define factory_h

#include "Config.h"

// factory default configuration
namespace Factory
{
	void LoadGlobal(GlobalConfig & global);
	void LoadPreset(uint8_t index, Preset & preset);
}

#endif
