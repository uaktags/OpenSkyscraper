#include "Application.h"

using namespace OT;


State::State(std::string name)
{
	active = false;
}

void State::activate()
{
	active = true;
//#ifdef BUILD_DEBUG
//	Rocket::Debugger::SetContext(gui.context);
//#endif
}

void State::deactivate()
{
	active = false;
}
