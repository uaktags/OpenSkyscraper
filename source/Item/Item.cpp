/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <cassert>
#include <algorithm>
#include <string>
#include "../Game.h"
#include "../Time.h"
#include "Item.h"
#include <cmath>
#include "Condo.h"
#include "YootCondo.h"
#include "Hotel.h"
#include "Office.h"

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

	// --- AnimPeple: occupants as animated person sprites ---
	// Suppression + pacing ported from OpenSkyWeb item.js render()/renderPeople
	// (ISSUE-024/035/037, commits 00f34e4, 77a9feb, 4145df7, 85f318b):
	//  - elevators and stairlikes pre-render people in their own animation
	//    frames (queues/cars, directional walking frames);
	//  - event venues (cinema/party hall) have pre-rendered crowds;
	//  - condos show dedicated night frames instead of standing figures.
	const std::string & itemId = prototype->id;
	bool sleepingCondo = false;
		if (itemId == "condo" || itemId == "yoot_condo") {
			if (const Condo * condo = dynamic_cast<const Condo *>(this))
			sleepingCondo = condo->isNightLighting();
			else if (const YootCondo * yc = dynamic_cast<const YootCondo *>(this))
			sleepingCondo = yc->isNightLighting();
		}
	const bool suppressOccupants =
		canHaulPeople()
		|| prototype->icon == ICON_FLOOR || itemId == "floor"
		|| itemId == "cinema" || itemId == "partyhall"
		|| sleepingCondo;
	if (!people.empty() && !underConstruction && !suppressOccupants)
	{
		renderPeople(target, statusTint, tinted);
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


// --- AnimPeple occupant overlay (OpenSkyWeb item.js renderPeople port) ---
// Frame pacing: the multi-activity sheets (office, hotel, event, guard,
// restaurant, housekeeper) hold each activity for several seconds with a
// gentle 1-2 Hz micro-animation toggle instead of strobing every frame;
// condos map residents to persona frames by Person type; the generic
// silhouette sheet keeps the continuous walking cycle.
void Item::renderPeople(sf::RenderTarget & target, const sf::Color & statusTint, bool tinted) const
{
	const std::string & id = prototype->id;

	std::vector<Person *> peopleArr;

	const Hotel * hotel = dynamic_cast<const Hotel *>(this);
	if (hotel)
	{
		// ISSUE-037: the room sheet's resting column pre-renders sleeping
		// guests; only awake occupants get a standing overlay figure.
		if (hotel->roomState != Hotel::kOccupied) return;
		for (People::const_iterator ip = people.begin(); ip != people.end(); ++ip)
			if ((*ip)->state != Person::kResting) peopleArr.push_back(*ip);
		if (peopleArr.empty()) return;
	}
	else
	{
		peopleArr.assign(people.begin(), people.end());
	}

	// Fine-dining restaurant: the background texture pre-renders seated
	// diners; the overlay is active waitstaff (1, or 2 at >=10 diners).
	if (id == "restaurant")
	{
		const size_t maxServers = (peopleArr.size() >= 10) ? 2 : 1;
		if (peopleArr.size() > maxServers) peopleArr.resize(maxServers);
	}
	// Cafes keep their pre-rendered interior; no animpeple overlay.
	if (id == "fastfood") return;

	// Occupied, lit office whose workers have not spawned yet: show a
	// subset of the assigned workers so the office looks populated.
	if (peopleArr.empty() && id == "office")
	{
		const Office * office = dynamic_cast<const Office *>(this);
if (office && office->isOccupied() && office->isLit())
{
peopleArr = office->getWorkerPeople(3);
		}
	}
	const int N = (int)peopleArr.size();
	if (N == 0) return;

	const int itemWidthPx = size.x * 8;
	const int frameW = 16;
	const int frameH = 24;
	const int stories = std::max(1, size.y);
	const int slotsPerStory = std::max(1, (N + stories - 1) / stories);
	const int marginPx = (prototype->icon == ICON_LOBBY) ? 16 : 4;
	const float minX = (float)(position.x * 8 + marginPx);
	const float maxX = (float)(position.x * 8 + itemWidthPx - marginPx - frameW);
	const int pxBase = std::abs(position.x % 17);

	int idx = 0;
	for (std::vector<Person *>::const_iterator ip = peopleArr.begin(); ip != peopleArr.end(); ++ip, ++idx)
	{
		Person * p = *ip;
		const char *sheet = "simtower/people";
		if (p->type == Person::kHousekeeper || id == "housekeeping") {
			sheet = "simtower/animpeple/housekeeper";
		} else if (p->type == Person::kSecurity || id == "security") {
			sheet = "simtower/animpeple/guard";
		} else if (id == "office") {
			sheet = "simtower/animpeple/office";
		} else if (id == "condo" || id == "yoot_condo") {
			sheet = "simtower/animpeple/condo";
		} else if (id == "hotel" || id == "hotel_single" || id == "hotel_double" || id == "hotel_suite") {
			sheet = "simtower/animpeple/hotel";
		} else if (id == "restaurant") {
			sheet = "simtower/animpeple/restaurant";
		} else if (id == "cinema" || id == "partyhall") {
			sheet = "simtower/animpeple/event";
		}

		bool genericPeopleSheet = (std::string(sheet) == "simtower/people");
		sf::Texture *peopleTex = &app->bitmaps[sheet];
		// Placeholder fallback (extraction not regenerated): use the generic
		// masked people sheet rather than raw unhandled BMPs.
		if (peopleTex->getSize().x > 1024) {
			peopleTex = &app->bitmaps["simtower/people"];
			genericPeopleSheet = true;
		}
		int frameCount = std::max(1u, peopleTex->getSize().x / frameW);

		int frame = 0;
		float slotJitter = 0.f;
		const double phase = std::fmod((p->animOffset != 0.0 ? p->animOffset : idx * 0.25) * 17.0 + idx * 2.37 + pxBase * 0.31, 1000.0);

		if (genericPeopleSheet)
		{
			// Generic walk sheet: 6-frame continuous walking cycle.
			frame = (int)std::fmod(game->time.absolute * 100.0 + p->animOffset * frameCount, (double)frameCount);
		}
		else if (id == "condo" || id == "yoot_condo")
		{
			// Condo resident persona mapping by Person type (27-frame sheet:
			// men 0-1, women 2-21, kids 22-24, parents/pets interleaved).
			const double condoPhase = std::fmod((p->animOffset != 0.0 ? p->animOffset : idx * 0.37) * 17.0 + idx * 3.1 + pxBase * 0.31, 1000.0);
			const long long cycleStep = (long long)std::floor(game->time.absolute * 8.0 + condoPhase);
			if (p->type == Person::kMan || p->type == Person::kSalesman)
			{
				const int manFrames[2] = {0, 1};
				frame = manFrames[std::abs((int)((cycleStep + idx * 3) % 2))];
			}
			else if (p->type == Person::kChild)
			{
				const int kidFrames[3] = {22, 23, 24};
				frame = kidFrames[std::abs((int)((cycleStep + idx * 5) % 3))];
			}
			else if (p->type == Person::kWomanWithChild1 || p->type == Person::kWomanWithChild2)
			{
				const int parentPairs[3][2] = {{20, 20}, {18, 19}, {4, 4}};
				const int pIdx = std::abs((int)((cycleStep * 5 + idx * 7) % 3));
				const int sub = (int)std::floor(std::fmod(game->time.absolute * 30.0 + condoPhase * 2.0, 2.0));
				frame = parentPairs[pIdx][sub];
			}
			else
			{
				const int womanActivities[9][2] = {
					{2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11},
					{12, 13}, {14, 15}, {16, 17}, {21, 21}};
				const int act = ((int)((cycleStep * 7 + idx * 5 + position.x * 3) % 9) + 9) % 9;
				const int sub = (int)std::floor(std::fmod(game->time.absolute * 40.0 + condoPhase * 2.0, 2.0));
				frame = womanActivities[act][sub];
			}
			// Distribute across living room / dining area / bedroom.
			const int slotOffset = ((idx * 31 + std::abs(position.x * 7)) % 7 - 3) * 4;
			slotJitter = (float)(slotOffset + (((std::abs((int)(cycleStep * 3 + idx * 7)) % 3) - 1) * 2));
		}
		else
		{
			// Multi-activity paired sheets: hold each activity for ~4-8
			// seconds (~12 changes per game day) with a ~1-2 Hz micro-
			// animation toggle, instead of a full-speed costume strobe.
			const long long cycleStep = (long long)std::floor(game->time.absolute * 12.0 + phase);
			const int pairCount = frameCount / 2;
			if (pairCount >= 2)
			{
				long long pairKey = cycleStep * 10007LL + (long long)(idx * 997) + (long long)(position.x * 13);
				int pairIndex = (int)(((pairKey % pairCount) + pairCount) % pairCount);
				int subframe = (int)std::floor(std::fmod(game->time.absolute * 60.0 + phase * 3.0, 2.0));
				frame = pairIndex * 2 + subframe;
			}
			else
			{
				long long key = cycleStep * 10007LL + (long long)(idx * 997);
				frame = (int)(((key % frameCount) + frameCount) % frameCount);
			}
			// Slight positional wander between activity cycles.
			slotJitter = (float)((((int)((cycleStep * 7 + idx * 13 + position.x) % 5) + 5) % 5) - 2) * 3.f;
		}
		frame = ((frame % frameCount) + frameCount) % frameCount;

		const int story = idx % stories;
		const int slot = idx / stories;
		float px = (float)(position.x * 8 + (slot + 0.5f) * itemWidthPx / slotsPerStory - frameW / 2.0f) + slotJitter;
		if (maxX >= minX) {
			px = std::max(minX, std::min(maxX, px));
		} else {
			px = (float)(position.x * 8 + (itemWidthPx - frameW) / 2.0f);
		}
		const float py = (float)(-(position.y + story) * 36);

		sf::Sprite ps(*peopleTex);
		ps.setTextureRect(sf::IntRect({frame * frameW, 0}, {frameW, frameH}));
		ps.setOrigin({0.f, (float)frameH});
		ps.setPosition({px, py});

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
