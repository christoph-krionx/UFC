#ifndef menu_h
#define menu_h

#include "Config.h"

class Controller;

/*
	Edit mode on the device, opened by holding switch 19 and 20.

	The bottom row is used as keys:
	switch 1 / 2	previous / next field
	switch 3 / 4	value down / up (hold to repeat)
	switch 5	back, unsaved changes are discarded
	switch 6	save

	Any other switch selects itself as the switch to edit.
*/
class Menu
{
	public:
		void Init(Controller * controller);
		bool Active() { return _active; }
		void Enter();
		void OnSwitch(uint8_t index, bool pressed);
		void Update();
		void RefreshLeds();

	private:
		bool TargetGlobal();
		SwitchConfig & TargetConfig();
		Message & MessageOf(uint8_t number);
		void AddField(uint8_t id, uint8_t sub);
		void AddMessageFields(uint8_t number, const Message & message, bool withOff);
		void BuildFields();
		void HandleKey(uint8_t key);
		void Adjust(int8_t delta);
		void SetGlobal(uint8_t index, bool global);
		void MarkDirty();
		void Save();
		void Exit();
		void Notice(const char * text);
		void Draw();

		Controller * _controller;
		bool _active;
		bool _dirtyPreset;
		bool _dirtyGlobal;

		uint8_t _target;	// 0: preset settings, n: switch n - 1
		uint8_t _fields[40];
		uint8_t _count;
		uint8_t _cursor;

		int8_t _held;
		uint32_t _nextRepeat;

		bool _noticeActive;
		uint32_t _noticeStart;
};

#endif
