#include "State.h"
#include "Application.h"

using namespace OT;

State::State(std::string name)
{
    snprintf(debugString, sizeof(debugString), "%s", name.c_str());
    active = false;
}

void State::activate()
{
	active = true;
#ifdef BUILD_DEBUG
	// TODO: TGUI debugger if available
	// Rocket::Debugger::SetContext(gui.context);
#endif
}

void State::deactivate()
{
	active = false;
}
