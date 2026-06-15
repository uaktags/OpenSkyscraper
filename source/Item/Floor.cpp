#include <cassert>
#include "../Game.h"
#include "Floor.h"

using namespace OT;
using namespace Item;

Floor::~Floor() {}

void Floor::init()
{
	Item::init();

	layer = -1;

	sf::Texture &floor = App->bitmaps["simtower/floor"];
	background.setTexture(floor);
	background.setTextureRect(sf::IntRect({0, 0}, {8, 36}));
	background.setOrigin({0.f, static_cast<float>(floor.getSize().y)});
	background.setScale({8.0f / floor.getSize().x, 36.0f / floor.getSize().y});
	background.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});

	ceiling.setTexture(floor);
	ceiling.setTextureRect(sf::IntRect({0, 0}, {8, 12}));
	ceiling.setOrigin({0.f, static_cast<float>(floor.getSize().y)});
	ceiling.setScale({8.0f / floor.getSize().x, 36.0f / floor.getSize().y});
	ceiling.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});

	interval.insert(position.x);
	interval.insert(getRect().maxX());

	updateSprite();
}

void Floor::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("width", size.x);
}

void Floor::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	size.x = xml.IntAttribute("width");
	updateSprite();
}

void Floor::updateSprite() {}

void Floor::render(sf::RenderTarget &target) const
{
	bool drawBackground = true;
	int left = 0;
	int right = 0;
	Sprite b = background;
	Sprite c = ceiling;
	for (std::multiset<int>::const_iterator i = interval.begin(); i != interval.end();)
	{
		if (drawBackground)
		{
			// Draw full floor sprite
			drawBackground = false;
			left = *i;
			++i;
			if (i == interval.end())
				break;
			right = *i;
			if (left == right)
				continue;

			b.setScale({(right - left) * 8.0f / b.getTextureRect().size.x, 1.f});
			b.setPosition({left * 8.0f, static_cast<float>(-getPosition().y * 36)});
			target.draw(b);
		}
		else
		{
			// Draw only ceiling sprite
			drawBackground = true;
			left = *i;
			++i;
			if (i == interval.end())
				break;
			right = *i;
			if (left == right)
				continue;

			c.setScale({(right - left) * 8.0f / c.getTextureRect().size.x, 1.f});
			c.setPosition({left * 8.0f, static_cast<float>(-getPosition().y * 36)});
			target.draw(c);
		}

		game->drawnSprites++;
	}
}
