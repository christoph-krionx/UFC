/*
	ufc - configurable midi foot controller for the UFC rev. 0 board
*/

#include "Controller.h"
#include "Protocol.h"

Controller controller;
Protocol protocol;

void setup()
{
	controller.Init();
	protocol.Init(&controller);
}

void loop()
{
	controller.Update();
	protocol.Update();
}
