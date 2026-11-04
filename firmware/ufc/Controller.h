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
		// hostIdle: no command of the editor is being received
		void Update(bool hostIdle);

		uint8_t PresetIndex();
		uint8_t PageIndex();
		bool MenuActive();
		void LoadPreset(uint8_t index);
		// reads the global configuration again after it was changed in the storage
		void ReloadGlobal();
		void FactoryReset();
		const CRGB * LedData() { return _leds.Data(); }

	private:
		bool IsGlobal(uint8_t page, uint8_t index);
		const SwitchConfig & ConfigOf(uint8_t page, uint8_t index);
		const SwitchConfig & ConfigOf(uint8_t index) { return ConfigOf(_page, index); }
		bool & On(uint8_t page, uint8_t index) { return _on[page][index]; }

		bool HasAction(uint8_t index);
		void OnPress(uint8_t index);
		void OnRelease(uint8_t index);
		void CheckHold();
		void RunAction(uint8_t number);
		void SetPage(uint8_t page);

		void SetTempo(uint16_t bpm);
		void Tap();
		void UpdateClock();
		bool IsTapTarget(uint8_t index, const SwitchConfig & config);

		void OnSwitch(uint8_t index, bool pressed);
		void Activate(const SwitchConfig & config, bool on);
		void ApplyLinks(const SwitchConfig & config);
		void SendMessages(const SwitchConfig & config, bool on);
		bool Send(const Message & message, bool on);
		void RefreshLeds();
		void Flash(const char * text);
		void FormatPresetText(char * line, bool withTempo);
		void ShowPresetLine();
		void ShowStatusLine();

		Switches _switches;
		Leds _leds;
		MidiOut _midi;
		Menu _menu;

		GlobalConfig _global;
		Preset _preset;
		uint8_t _presetIndex;
		uint8_t _page;

		bool _on[NUM_PAGES][NUM_SWITCHES];
		bool _pressed[NUM_SWITCHES];
		// a switch with an action reacts on release, unless the action has used the press
		bool _consumed[NUM_SWITCHES];
		uint32_t _pressStart[NUM_SWITCHES];
		bool _actionOn[NUM_ACTIONS];
		uint8_t _lastPc[16];	// per channel, 0xFF: unknown

		// the group switch pressed last, pressing it again is tap tempo
		uint8_t _lastGroup;
		uint8_t _lastGroupPage;

		uint16_t _tempo;	// 0: clock stopped
		uint32_t _tickInterval;	// us
		uint32_t _nextTick;
		uint8_t _tick;
		bool _beat;
		uint32_t _lastTap;
		uint32_t _tapSum;
		uint8_t _tapCount;

		bool _flashActive;
		uint32_t _flashStart;

		bool _presetSavePending;
		uint32_t _presetChanged;

		bool _menuHold;
		uint32_t _menuHoldStart;
};

#endif
