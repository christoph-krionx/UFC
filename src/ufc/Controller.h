#ifndef controller_h
#define controller_h

#include "Config.h"
#include "Switches.h"
#include "Leds.h"
#include "MidiOut.h"
#include "Menu.h"

class Controller
{
	friend class Menu;

	public:
		void Init();
		void Update();

		uint8_t PresetIndex();
		bool MenuActive();
		void LoadPreset(uint8_t index);
		// reads the global configuration again after it was changed in the storage
		void ReloadGlobal();
		void FactoryReset();
		const CRGB * LedData() { return _leds.Data(); }

	private:
		const SwitchConfig & ConfigOf(uint8_t index);
		void OnSwitch(uint8_t index, bool pressed);
		void SendMessages(const SwitchConfig & config, bool on);
		bool Send(const Message & message, bool on);
		void RefreshLeds();
		void ShowPresetLine();
		void ShowStatusLine();

		Switches _switches;
		Leds _leds;
		MidiOut _midi;
		Menu _menu;

		GlobalConfig _global;
		Preset _preset;
		uint8_t _presetIndex;

		bool _on[NUM_SWITCHES];
		bool _pressed[NUM_SWITCHES];

		bool _flashActive;
		uint32_t _flashStart;

		bool _presetSavePending;
		uint32_t _presetChanged;

		bool _menuHold;
		uint32_t _menuHoldStart;
};

#endif
