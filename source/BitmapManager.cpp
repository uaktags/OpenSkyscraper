#include "Application.h"
#include "BitmapManager.h"
#include "DataManager.h"
#include <SFML/Graphics/Image.hpp>

using namespace OT;

static sf::Vector2u fallbackBitmapSize(Path name)
{
	const std::string path = name.str();
	if (path.find("simtower/sky") == 0 || path.find("simtower\\sky") == 0)
		return {1920, 360};
	if (path.find("simtower/ui/time/bg") == 0 || path.find("simtower\\ui\\time\\bg") == 0)
		return {431, 41};
	if (path.find("simtower/ui/time/rating") == 0 || path.find("simtower\\ui\\time\\rating") == 0)
		return {108, 22};
	if (path.find("simtower/ui/toolbox/tools") == 0 || path.find("simtower\\ui\\toolbox\\tools") == 0)
		return {64, 21};
	if (path.find("simtower/ui/toolbox/items") == 0 || path.find("simtower\\ui\\toolbox\\items") == 0)
		return {800, 32};
	return {2048, 512};
}

static bool loadFallbackBitmap(Path name, sf::Texture &dst)
{
	const sf::Vector2u size = fallbackBitmapSize(name);
	sf::Image image(size, sf::Color(70, 76, 84));
	const std::string path = name.str();
	unsigned int tint = 0;
	for (std::string::const_iterator i = path.begin(); i != path.end(); i++)
		tint = tint * 33 + static_cast<unsigned char>(*i);
	const sf::Color a(50 + tint % 90, 70 + (tint / 7) % 90, 90 + (tint / 17) % 90);
	const sf::Color b(120 + (tint / 3) % 80, 110 + (tint / 11) % 80, 100 + (tint / 19) % 80);
	for (unsigned int y = 0; y < size.y; y++)
	{
		for (unsigned int x = 0; x < size.x; x++)
		{
			const bool stripe = ((x / 16 + y / 16) % 2) == 0;
			image.setPixel({x, y}, stripe ? a : b);
		}
	}
	return dst.loadFromImage(image);
}

bool BitmapManager::load(Path name, sf::Texture & dst)
{
	// Fetch the possible locations for this bitmap. Dumped SimTower resources
	// live without the leading "simtower/" prefix and with a .png extension.
	DataManager::Paths paths = app->data.paths(Path("bitmaps") + name);
	if (name.str().find(internal_path) == 0) {
		DataManager::Paths strippedPaths = app->data.paths(Path("bitmaps") + name.str().substr(internal_path.size()));
		paths.insert(paths.end(), strippedPaths.begin(), strippedPaths.end());
	}
	paths.push_back(name);

	// Try to load the bitmap.
	bool success = false;
	for (int i = 0; i < paths.size() && !success; i++) {
		success = dst.loadFromFile(paths[i].c_str());
		if (!success && paths[i].str().find('.') == std::string::npos) {
			success = dst.loadFromFile((paths[i].str() + ".png").c_str());
		}
	}

	//Return success.
	if (success) {
		LOG(DEBUG,   "loaded bitmap '%s'", name.c_str());
	} else {
		success = loadFallbackBitmap(name, dst);
		usedFallback = true;
		LOG(WARNING, "using fallback bitmap '%s'", name.c_str());
	}
	return success;
}
