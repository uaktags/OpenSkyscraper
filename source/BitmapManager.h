#pragma once
#include <string>
#include <SFML/Graphics/Texture.hpp>
#include "ResourceManager.h"

namespace OT
{
	class Application;

	class BitmapManager : public ResourceManager<sf::Texture>
	{
	public:
		BitmapManager(Application * app) : ResourceManager<sf::Texture>(app), usedFallback(false), internal_path("simtower") { internal_path.push_back(Path::SEPARATOR); }
		bool load(Path name, sf::Texture & dst);
		bool usedFallback;
	private:
		std::string internal_path;
	};
}
