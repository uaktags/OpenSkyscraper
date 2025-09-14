#pragma once
#include <SFML/Graphics/Font.hpp>
#include "ResourceManager.h"
#include "Path.h"

namespace OT
{
	class Application;
	
	class FontManager : public ResourceManager<sf::Font>
	{
	public:
		FontManager(Application * app) : ResourceManager<sf::Font>(app) {}
	
	protected: 
		bool load(Path name, sf::Font & dst) override;
	};
}
