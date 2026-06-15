#pragma once

#include <SFML/Window.hpp>
#include <string>


namespace OT
{
	class Application;
	
	/// Application uses State classes to draw stuff.
	class State
	{
	public:
		char debugString[512];
		State(std::string name);
		virtual ~State() = default;
		
		//GUI gui;
		
		virtual void activate();
		virtual bool handleEvent(sf::Event & event) { return false; }
		virtual void advance(double dt) {}
		virtual void deactivate();
		virtual void reloadGUI() {}
		
		bool isActive() { return active; }
		
	private:
		bool active;
	};
}
