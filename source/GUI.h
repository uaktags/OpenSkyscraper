#pragma once

#include <Rocket/Core.h>
#include <SFML/Graphics/RenderWindow.hpp>
#include <string>

#include "Path.h"

namespace OT
{
	class GUIManager;
	
	class GUI
	{
	public:
		GUIManager * manager;
		Rocket::Core::Context * context;
		float uiScale; // UI scaling factor (1.0 = normal)
		
		GUI(std::string name, GUIManager * manager);
		~GUI();
		
		bool handleEvent(sf::Event & event);
		void draw();
		
		// UI scale control. Must be > 0. Changing the scale updates the
		// Rocket context dimensions so documents render at the requested size.
		void setUIScale(float scale);
		float getUIScale() const;
		
		Rocket::Core::ElementDocument * loadDocument(Path path);
	};
}