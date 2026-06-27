/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <cassert>
#include <algorithm>
#include <string>
#include "../Game.h"
#include "../Time.h"
#include "Item.h"

using namespace OT::Item;
using OT::Person;
using OT::Sprite;
using OT::int2;
using OT::rectd;
using OT::recti;
using std::string;

double Item::constructionDuration() const
{
	const std::string & id = prototype->id;
	bool isInstant = (id == "lobby" || id == "floor" || id == "stairs" ||
	                  id == "escalator" || id == "parking" ||
	                  id.compare(0, 9, "elevator-") == 0);
	if (isInstant)
		return 0.0;

	// 1.0 to 2.5 seconds depending on width, at normal game speed.
	return (1.0 + 0.1 * size.x) * OT::Time::kBaseSpeed;
}

Item::~Item()
{
	sprites.clear();
	while (!people.empty())
		removePerson(*people.begin());
}

void Item::setPosition(int2 p)
{
	if (position != p)
	{
		position.x = p.x;
		position.y = p.y;
	}
}

int2 Item::getPosition() const
{
	return position;
}

int2 Item::getPositionPixels() const
{
	return int2(position.x * 8, position.y * 36);
}

recti Item::getRect() const
{
	return recti(getPosition(), int2(getSize().x, getSize().y));
}

recti Item::getPixelRect() const
{
	return recti(getPositionPixels(), int2(getSizePixels().x, getSizePixels().y));
}

void Item::addSprite(Sprite *sprite)
{
	assert(sprite);
	sprites.insert(sprite);
}

void Item::removeSprite(Sprite *sprite)
{
	assert(sprite);
	sprites.erase(sprite);
}

void Item::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
	render(target);
}

void Item::render(sf::RenderTarget &target) const
{
	bool showConstruction = underConstruction;
	sf::Color statusTint = sf::Color::White;
	if (game->statusMode == Game::kHotel)
	{
		showConstruction = false;
		const std::string & id = prototype->id;
		bool isHotel = (id == "hotel_single" || id == "hotel_double" ||
		                id == "hotel_suite"  || id == "hotel");
		if (!isHotel)
			statusTint = sf::Color(110, 110, 110, 160); // grey out non-hotels
	}

	if (showConstruction)
	{
		const int2 p = getPositionPixels();
		const sf::Vector2u s = getSizePixels();

		double duration = constructionDuration();
		double progress = 1.0;
		if (duration > 0.0)
		{
			double elapsed = game->time.absolute - (constructionEndTime - duration);
			progress = elapsed / duration;
			if (progress < 0.0) progress = 0.0;
			if (progress > 1.0) progress = 1.0;
		}

		sf::Texture &gridTex = app->bitmaps["simtower/construction/grid"];
		sf::Texture &solidTex = app->bitmaps["simtower/construction/solid"];
		sf::Texture &workerTex = app->bitmaps["simtower/construction/worker"];

		int H = static_cast<int>(24.f * (1.0f - progress));
		int N = std::max(1, static_cast<int>(size.x / 4));
		int animIndex = static_cast<int>(game->time.absolute * 1500.0);

		for (int story = 0; story < size.y; story++)
		{
			int y_offset = story * 36;

			// 1. Draw Grid (scaffolding) for this story
			sf::Sprite gridSprite(gridTex);
			gridSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(s.x), 36}));
			gridSprite.setColor(game->lighting.compose(sf::Color::White));
			gridSprite.setPosition({static_cast<float>(p.x), static_cast<float>(-(p.y + y_offset + 36))});
			target.draw(gridSprite);

			// 2. Draw Workers for this story
			sf::Sprite workerSprite(workerTex);
			workerSprite.setOrigin({8.f, 24.f});
			workerSprite.setColor(game->lighting.compose(sf::Color::White));
			for (int i = 0; i < N; i++)
			{
				float workerX = p.x + (i + 0.5f) * (s.x / N);
				float workerY = -(p.y + y_offset);
				int frame = (animIndex + i * 17 + story * 29) % 6;
				workerSprite.setTextureRect(sf::IntRect({frame * 16, 0}, {16, 24}));
				workerSprite.setPosition({workerX, workerY});
				target.draw(workerSprite);
			}

			// 3. Draw Solid shutter for this story
			if (H > 0)
			{
				sf::Sprite solidSprite(solidTex);
				solidSprite.setTextureRect(sf::IntRect({0, 0}, {static_cast<int>(s.x), H}));
				solidSprite.setColor(game->lighting.compose(sf::Color::White));
				solidSprite.setPosition({static_cast<float>(p.x), static_cast<float>(-(p.y + y_offset + 24))});
				target.draw(solidSprite);
			}
		}

		// 4. Draw outline around the construction footprint
		sf::RectangleShape outline({static_cast<float>(s.x), static_cast<float>(s.y)});
		outline.setPosition({static_cast<float>(p.x),
		                     static_cast<float>(-(p.y + static_cast<int>(s.y)))});
		outline.setFillColor(sf::Color::Transparent);
		outline.setOutlineColor(game->lighting.compose(sf::Color(90, 70, 30)));
		outline.setOutlineThickness(1.f);
		target.draw(outline);

		game->drawnSprites++;
		return;
	}

	// Apply the global Lighting tint to each sprite. Sprites keep their
	// own color elsewhere (often white, sometimes a subclass-specific
	// tint), so we save/restore around the draw to avoid compounding the
	// tint across frames. Phase 3.4.
	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255) || statusTint != sf::Color::White);
	for (SpriteSet::iterator s = sprites.begin(); s != sprites.end(); s++)
	{
		game->drawnSprites++;
		if (tinted)
		{
			const sf::Color orig = (*s)->getColor();
			sf::Color composed = game->lighting.compose(orig);
			if (statusTint != sf::Color::White)
			{
				composed.r = (composed.r * statusTint.r) / 255;
				composed.g = (composed.g * statusTint.g) / 255;
				composed.b = (composed.b * statusTint.b) / 255;
				composed.a = (composed.a * statusTint.a) / 255;
			}
			(*s)->setColor(composed);
			target.draw(**s);
			(*s)->setColor(orig);
		}
		else
		{
			target.draw(**s);
		}
	}

	// --- AnimPeple: render occupants as animated person sprites ---
	// The `simtower/people` texture is a 96x120 sheet: 6 walking frames
	// (16px each) x 5 color rows (24px each). We use row 0 (transparent
	// background) and distribute people across the item's footprint.
	if (!people.empty() && !underConstruction)
	{
		const int itemWidthPx = size.x * 8;
		const int frameW = 16;
		const int frameH = 24;
		const int N = static_cast<int>(people.size());
		const int stories = std::max(1, size.y);
		const int slotsPerStory = std::max(1, (N + stories - 1) / stories);
		int idx = 0;
		for (People::iterator ip = people.begin(); ip != people.end(); ++ip, ++idx)
		{
			Person * p = *ip;
			const char *sheet = "simtower/people";
			const std::string &id = prototype->id;
			if (p->type == Person::kHousekeeper) {
				sheet = "simtower/animpeple/housekeeper";
			} else if (id == "office") {
				sheet = "simtower/animpeple/office";
			} else if (id == "condo" || id == "yoot_condo") {
				sheet = "simtower/animpeple/condo";
			} else if (id == "hotel" || id == "hotel_single" || id == "hotel_double" || id == "hotel_suite") {
				sheet = "simtower/animpeple/hotel";
			} else if (id == "restaurant" || id == "fastfood") {
				sheet = "simtower/animpeple/restaurant";
			} else if (id == "cinema" || id == "partyhall") {
				sheet = "simtower/animpeple/event";
			}

			bool genericPeopleSheet = (std::string(sheet) == "simtower/people");
			sf::Texture *peopleTex = &app->bitmaps[sheet];
			// If the named extraction hasn't been regenerated yet, BitmapManager
			// returns a large placeholder fallback. Use the generic masked people
			// sheet rather than raw unhandled BMPs, which lack transparency.
			if (peopleTex->getSize().x > 1024) {
				peopleTex = &app->bitmaps["simtower/people"];
				genericPeopleSheet = true;
			}

			int frameCount = std::max(1u, peopleTex->getSize().x / frameW);
			// Walking frames, cycling with per-person offset.
			int frame = static_cast<int>(game->time.absolute * 400.0 + p->animOffset * frameCount) % frameCount;
			// Distribute people evenly across the item footprint, including
			// multi-story tenants. This approximates the original Draw*AP
			// per-location placement until individual animation maps are decoded.
			int story = idx % stories;
			int slot = idx / stories;
			float px = position.x * 8 + (slot + 0.5f) * itemWidthPx / slotsPerStory - frameW / 2.0f;
			float py = -(position.y + story) * 36;

			sf::Sprite ps(*peopleTex);
			ps.setTextureRect(sf::IntRect({frame * frameW, 0}, {frameW, frameH}));
			ps.setOrigin({0.f, static_cast<float>(frameH)});
			ps.setPosition({px, py});

			// Full-color AnimPeple sheets should keep their original palette.
			// Only the generic silhouette fallback is stress-colored.
			sf::Color color = sf::Color::White;
			if (genericPeopleSheet) {
				color = sf::Color::Black;
				if (p->stress > 80) color = sf::Color(255, 0, 0);
				else if (p->stress > 40) color = sf::Color(255, 128, 128);
			}
			color = game->lighting.compose(color);
			if (statusTint != sf::Color::White)
			{
				color.r = (color.r * statusTint.r) / 255;
				color.g = (color.g * statusTint.g) / 255;
				color.b = (color.b * statusTint.b) / 255;
				color.a = (color.a * statusTint.a) / 255;
			}
			ps.setColor(color);
			target.draw(ps);
			game->drawnSprites++;
		}
	}

	if (!canHaulPeople() && position.y != 0 && prototype->icon != ICON_FLOOR && lobbyRoute.empty())
	{
		Sprite noroute;
		noroute.setTexture(app->bitmaps["noroute.png"]);
		sf::Vector2u size = noroute.getTexture().getSize();
		noroute.setOrigin({size.x / 2.f, size.y / 2.f});
		size = getSize();
		noroute.setPosition({size.x / 2.f, -size.y / 2.f});
		if (tinted)
		{
			sf::Color composed = game->lighting.compose(sf::Color::White);
			if (statusTint != sf::Color::White)
			{
				composed.r = (composed.r * statusTint.r) / 255;
				composed.g = (composed.g * statusTint.g) / 255;
				composed.b = (composed.b * statusTint.b) / 255;
				composed.a = (composed.a * statusTint.a) / 255;
			}
			noroute.setColor(composed);
		}
		target.draw(noroute);
	}
}

void Item::encodeXML(tinyxml2::XMLPrinter &xml)
{
	xml.PushAttribute("type", prototype->id.c_str());
	xml.PushAttribute("x", position.x);
	xml.PushAttribute("y", position.y);
	xml.PushAttribute("evaluation", evaluation);
	if (underConstruction)
	{
		xml.PushAttribute("underConstruction", underConstruction);
		xml.PushAttribute("constructionEndTime", constructionEndTime);
	}
}

void Item::decodeXML(tinyxml2::XMLElement &xml)
{
	evaluation = xml.DoubleAttribute("evaluation", 50.0);
	// Saved state wins over the Factory's "fresh placement" default so a
	// save mid-construction restores correctly (and finished items stay
	// finished after a reload).
	underConstruction = xml.BoolAttribute("underConstruction", false);
	constructionEndTime = xml.DoubleAttribute("constructionEndTime", 0.0);
}

void Item::addPerson(Person *p)
{
	assert(p);
	assert(!p->at);
	p->at = this;
	people.insert(p);
}

void Item::removePerson(Person *p)
{
	assert(p);
	assert(p->at == this);
	p->at = NULL;
	people.erase(p);
}

rectd Item::getMouseRegion()
{
	int2 p = getPositionPixels();
	sf::Vector2u s = getSizePixels();
	return rectd(p.x, p.y, s.x, s.y);
}

bool Item::containsPoint(const double2 & pt)
{
	return getMouseRegion().containsPoint(pt);
}

void Item::updateRoutes()
{
	if (!canHaulPeople() && position.y != 0)
	{
		lobbyRoute = game->findRoute(game->mainLobby, this);
	}
	else
	{
		lobbyRoute.clear();
	}
}
