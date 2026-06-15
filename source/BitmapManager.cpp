/* Copyright (c) 2026 OpenSkyscraper contributors */
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
	if (path.find("simtower/ui/toolbox/speed") == 0 || path.find("simtower\\ui\\toolbox\\speed") == 0)
		return {92, 48};
	return {2048, 512};
}

static bool loadFallbackBitmap(Path name, sf::Texture &dst)
{
	const sf::Vector2u size = fallbackBitmapSize(name);
	const std::string path = name.str();
	if (path.find("simtower/ui/toolbox/speed") == 0 || path.find("simtower\\ui\\toolbox\\speed") == 0)
	{
		sf::Image image(size, sf::Color::Transparent);
		const sf::Color face[2] = {sf::Color(214, 214, 214), sf::Color(68, 68, 68)};
		const sf::Color light[2] = {sf::Color::White, sf::Color(112, 112, 112)};
		const sf::Color shadow[2] = {sf::Color(84, 84, 84), sf::Color(18, 18, 18)};
		const sf::Color glyph[2] = {sf::Color(20, 20, 20), sf::Color::White};

		auto fillRect = [&](unsigned int x, unsigned int y, unsigned int w, unsigned int h, sf::Color color) {
			for (unsigned int py = y; py < y + h; py++)
				for (unsigned int px = x; px < x + w; px++)
					image.setPixel({px, py}, color);
		};
		auto fillTriangle = [&](int ax, int ay, int bx, int by, int cx, int cy, sf::Color color) {
			const int minX = std::min(ax, std::min(bx, cx));
			const int maxX = std::max(ax, std::max(bx, cx));
			const int minY = std::min(ay, std::min(by, cy));
			const int maxY = std::max(ay, std::max(by, cy));
			const int area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
			for (int y = minY; y <= maxY; y++)
			{
				for (int x = minX; x <= maxX; x++)
				{
					const int w0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
					const int w1 = (cx - bx) * (y - by) - (cy - by) * (x - bx);
					const int w2 = (ax - cx) * (y - cy) - (ay - cy) * (x - cx);
					if ((area >= 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
					    (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
						image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)}, color);
				}
			}
		};
		auto drawPlay = [&](unsigned int ox, unsigned int oy, unsigned int count, sf::Color color) {
			for (unsigned int i = 0; i < count; i++)
			{
				const int x = static_cast<int>(ox + 5 + i * 5);
				const int y = static_cast<int>(oy + 5);
				fillTriangle(x, y, x, y + 14, x + 8, y + 7, color);
			}
		};

		for (unsigned int row = 0; row < 2; row++)
		{
			for (unsigned int index = 0; index < 4; index++)
			{
				const unsigned int x = index * 23;
				const unsigned int y = row * 24;
				fillRect(x, y, 23, 24, face[row]);
				fillRect(x, y, 23, 1, light[row]);
				fillRect(x, y, 1, 24, light[row]);
				fillRect(x, y + 23, 23, 1, shadow[row]);
				fillRect(x + 22, y, 1, 24, shadow[row]);

				if (index == 0)
				{
					fillRect(x + 6, y + 5, 4, 14, glyph[row]);
					fillRect(x + 13, y + 5, 4, 14, glyph[row]);
				}
				else
				{
					drawPlay(x, y, index, glyph[row]);
				}
			}
		}
		return dst.loadFromImage(image);
	}
	sf::Image image(size, sf::Color(70, 76, 84));
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
