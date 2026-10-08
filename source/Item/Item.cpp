/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <cassert>
#include <algorithm>
#include <string>
#include "../Game.h"
#include "../Time.h"
#include "../YootItemArt.h"
#include "../YootWorldArt.h"
#include "Item.h"
#include <cmath>
#include "Condo.h"
#include "YootCondo.h"
#include "FastFood.h"
#include "Restaurant.h"
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

		// Skin::Yoot: T2WorldDef::DrawBuild tiles the world's 1004
		// `BuildPrc` over every row, scaffolding (pattern 0) while the
		// status is 0..1 and the boarded-up crates (1) after.
		if (const YootWorldArt * art = game->yootSkinArt())
		{
			const YootPluginView * v = yootPluginView(game->app, yootWorldPluginFile(art->worldId()),
				sf::Color::Transparent, 1004, 1004, "BuildPrc");
			if (v && v->bands.size() >= 2)
			{
				const sf::IntRect band = v->band(progress < 0.2 ? 0 : 1);
				for (int story = 0; story < size.y; story++)
					yootDrawLoop(target, *v, band, (float)p.x, (float)-(p.y + (story + 1) * 36), (float)s.x);
				game->drawnSprites += size.y;
				return;
			}
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
		|| itemId.compare(0, 3, "yt_") == 0  // YootTenant draws its PADF staff
		|| sleepingCondo;
	if (!people.empty() && !underConstruction && !suppressOccupants)
	{
		renderPeople(target, statusTint, tinted);
	}

	if (!canHaulPeople() && position.y != 0 && prototype->icon != ICON_FLOOR && lobbyRouteComputed && lobbyRoute.empty())
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
	// YootTower-only attributes. The values keep their defaults under the
	// SimTower/OpenSky rules, so gating on the ruleset keeps those saves
	// byte-identical to the pre-YootTower format.
	if (game && game->activeRules.profile().yootWorld)
	{
		if (yootEstimate != 0)
			xml.PushAttribute("yootEstimate", yootEstimate);
		if (yootBuildDay >= 0)
			xml.PushAttribute("buildDay", yootBuildDay);
		if (yootPeopleEstimate != 300)
			xml.PushAttribute("yootPeopleEstimate", yootPeopleEstimate);
	}
	if (underConstruction)
	{
		xml.PushAttribute("underConstruction", underConstruction);
		xml.PushAttribute("constructionEndTime", constructionEndTime);
	}
		}

void Item::decodeXML(tinyxml2::XMLElement &xml)
{
	evaluation = xml.DoubleAttribute("evaluation", 50.0);
	yootEstimate = xml.IntAttribute("yootEstimate", 0);
	yootBuildDay = xml.IntAttribute("buildDay", -1);
	yootPeopleEstimate = xml.IntAttribute("yootPeopleEstimate", 300);
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
	// Layout changed: invalidate the cached lobby route. The actual A* search
	// is deferred to computeLobbyRoute(), drained from Game's time-budgeted
	// route build queue - eagerly recomputing every room's route in one go
	// froze large towers for minutes on load (one search per room).
	lobbyRouteComputed = false;
	lobbyRoute.clear();
}

void Item::computeLobbyRoute()
{
	if (!canHaulPeople() && position.y != 0)
	{
		lobbyRoute = game->findRoute(game->mainLobby, this);
	}
	else
	{
		lobbyRoute.clear();
	}
	lobbyRouteComputed = true;
}


// --- AnimPeple occupant overlay (SIMTOWER.EXE AnimPeple, seg 1028) ---
// Tenant interiors draw the EXE's fixed occupant bands: offices 6 workers,
// condos 3 residents, hotels 2 guests in singles / 3 in twins & suites,
// and fast food / restaurants a 2-strong staff pair each over their pre-
// rendered interiors (concilia-tower game.c occ_roll_* after
// AnimateOfficePeople 1028:0902, AnimateCondoPeople 1028:0FEB and
// AnimateHotelGuests 1028:12C5). Poses are frame codes into the
// concatenated type-8 people band (sheets 0x85E8..0x85EE) re-rolled every
// 16 sim ticks, staggered per building so the tower does not twitch in
// sync. People merely passing through (lobbies, floors, cars) keep the
// generic walking cycle.
namespace {

// Fallback segmentation of the people band (used only when the loader's
// concatenated simtower/animpeple/band sheet is unavailable): 16x24 frames
// per segment, widths byte-verified against the NE bitmaps
// (480/432/96/160/112/256/48 px). Segment semantics per the loader's
// corrected assignment: 0x85EB is the fast-food staff strip (hotel guests
// actually live in the 0x85ED "event" and 0x85EE "housekeeper" segments).
const struct AnimPepleSegment { const char *sheet; int first; int count; } kAnimPepleBand[] = {
	{"simtower/animpeple/office",       0x00, 30},
	{"simtower/animpeple/condo",        0x1E, 27},
	{"simtower/animpeple/construction", 0x39, 6},
	{"simtower/animpeple/fastfood",     0x3F, 10},
	{"simtower/animpeple/restaurant",   0x49, 7},
	{"simtower/animpeple/event",        0x50, 16},
	{"simtower/animpeple/housekeeper",  0x60, 3},
};

// Deterministic roll stream for one re-pose: seeded from the building's
// position and the 16-tick step so a band holds its pose for 16 sim ticks
// and re-rolls as a whole (game.c game_animate_occupants; the EXE keys the
// stagger on the person handle, AnimPeple @0636 - hashing the position
// scatters neighbours the same way).
unsigned animPepleSeed(int x, int y, long long step)
{
	unsigned h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)step * 83492791u;
	h ^= h >> 13;
	h *= 0x5bd1e995u;
	h ^= h >> 15;
	return h ? h : 1;
}

} // namespace

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

	// --- EXE occupant bands (fixed figure count per venue) ---
	// Retail venues draw their permanent staff pair only while the doors
	// are open (the shuttered storefront shows nobody); residential and
	// office bands draw while occupants are home.
	int bandCount = 0;
	int bandFrame[6] = {0, 0, 0, 0, 0, 0};
	int bandX[6] = {0, 0, 0, 0, 0, 0};
	const bool staffVenue = (id == "fastfood" || id == "restaurant");
	const bool bandVenue = staffVenue || id == "office" || id == "condo" || id == "yoot_condo" ||
		id == "hotel" || id == "hotel_single" || id == "hotel_double" || id == "hotel_suite";
	bool venueOpen = true;
	if (staffVenue)
	{
		const FastFood * ff = dynamic_cast<const FastFood *>(this);
		const Restaurant * rr = dynamic_cast<const Restaurant *>(this);
		venueOpen = (ff && ff->open) || (rr && rr->open);
	}
	if (bandVenue && (staffVenue ? venueOpen : !peopleArr.empty()))
	{
		// Re-pose cadence: the EXE re-rolls each tenant every 16 frames of
		// its 2400-frame day, staggered per tenant. Our clock advances 2600
		// stages per day (jcranmer TDT model), so a step is 1/16th of that,
		// staggered by a per-building phase.
		const int kAnimPepleReposeTicks = 16; // EXE AnimPeple &15 mask
		const int bandPhase = std::abs(position.x * 31 + position.y * 17) & (kAnimPepleReposeTicks - 1);
		const long long bandStep = (long long)std::floor((game->time.absolute * 2600.0 + bandPhase) / kAnimPepleReposeTicks);

		unsigned seed = animPepleSeed(position.x, position.y, bandStep);
		// occ_roam / occ_clamp (game.c): x in cells, rand % (max(3,width)-1),
		// then clamped so the 2-cell-wide figure stays inside the unit.
		const int roamSpan = std::max(3, size.x) - 1;
		struct BandRoller
		{
			unsigned s;
			int span;
			int roll(int n) { s = s * 1664525u + 1013904223u; return (int)((s >> 16) % (unsigned)n); }
			int roam() { return roll(span); }
		};
		BandRoller roll = {seed, roamSpan};

		if (id == "office")
		{
			// AnimateOfficePeople 1028:0902 - 6 workers; the desk layout
			// forks on the decor variant (the sprite's texture row, see
			// Office::updateSprite: rows 0-5 occupied, 6 vacant).
			int variant = 0;
			if (!sprites.empty())
				variant = (*sprites.begin())->getTextureRect().position.y / 24;
			if (variant > 5) variant = 0;
			bandCount = 6;
			if (variant < 2)
			{
				// Three desk-sitters at fixed positions.
				bandX[0] = variant + 1; bandFrame[0] = 0x0E + roll.roll(2);
				bandX[1] = variant + 2; bandFrame[1] = 0x10 + roll.roll(2);
				bandX[2] = variant + 3; bandFrame[2] = 0x0C + roll.roll(2);
			}
			else
			{
				bandX[0] = 7;             bandFrame[0] = 0x0A + roll.roll(2);
				const int f1 = 6 + roll.roll(4);
				bandX[1] = (f1 >= 8) ? 4 : roll.roam(); bandFrame[1] = f1;
				const int f2 = 2 + roll.roll(4);
				bandX[2] = (f2 >= 4) ? 2 : roll.roam(); bandFrame[2] = f2;
			}
			bandX[3] = roll.roam(); bandFrame[3] = roll.roll(2);
			bandX[4] = roll.roam(); bandFrame[4] = 0x14 + roll.roll(4);
			bandX[5] = roll.roam(); bandFrame[5] = 0x12 + roll.roll(2);
		}
		else if (id == "condo" || id == "yoot_condo")
		{
			// AnimateCondoPeople 1028:0FEB - 3 residents.
			const double hourNow = game->time.hour;
			const bool weekend = game->time.isHoliday();
			const bool daylit = (hourNow < 17.0);
			const bool nightTime = (hourNow >= 21.0 || hourNow < 5.0);
			bandCount = 3;
			bandX[0] = roll.roam();
			bandFrame[0] = weekend ? 0x22 : (daylit ? 0x1E : 0x20 + roll.roll(2));
			const int f1 = 0x29 + roll.roll(9);
			bandX[1] = (f1 == 0x31) ? 1 : roll.roam(); // pose tied to the left end
			bandFrame[1] = f1;
			bandX[2] = roll.roam();
			bandFrame[2] = nightTime ? 0x36 + roll.roll(3) : 0x32 + roll.roll(4); // dog band at night
		}
		else if (id == "hotel" || id == "hotel_single" || id == "hotel_double" || id == "hotel_suite")
		{
			// AnimateHotelGuests 1028:12C5 - 2 sprites in singles, 3 in
			// twin/suite rooms.
			const double hourNow = game->time.hour;
			const bool daylit = (hourNow < 17.0);
			const bool nightTime = (hourNow >= 21.0 || hourNow < 5.0);
			bandCount = (prototype->variant == Hotel::kSingle) ? 2 : 3;
			bandX[0] = roll.roam(); bandFrame[0] = 0x60 + roll.roll(3);
			bandX[1] = roll.roam();
			bandFrame[1] = daylit ? 0x50 + roll.roll(2) : 0x52 + roll.roll(9);
			if (bandCount == 3)
			{
				bandX[2] = roll.roam();
				bandFrame[2] = (!daylit && nightTime) ? 0x5F /* asleep */ : 0x5B + roll.roll(4);
			}
		}
		else if (id == "fastfood")
		{
			// AnimateFastFoodStaff - both behind the counter (left 3 cells).
			bandCount = 2;
			bandX[0] = roll.roll(3); bandFrame[0] = 0x3F + roll.roll(2);
			bandX[1] = roll.roll(3); bandFrame[1] = 0x41 + roll.roll(2);
		}
		else if (id == "restaurant")
		{
			// AnimateRestaurantStaff - both behind the counter (left 3 cells).
			bandCount = 2;
			bandX[0] = roll.roll(3); bandFrame[0] = 0x49 + roll.roll(3);
			bandX[1] = roll.roll(3); bandFrame[1] = 0x4C + roll.roll(4);
		}

		// occ_clamp: keep the 2-cell-wide figure inside the unit.
		const int maxX = std::max(0, size.x - 2);
		for (int k = 0; k < bandCount; k++)
			if (bandX[k] > maxX) bandX[k] = maxX;
	}

	const int N = (bandCount > 0) ? bandCount : (int)peopleArr.size();
	if (N == 0) return;

	const int itemWidthPx = size.x * 8;
	const int frameW = 16;
	const int frameH = 24;
	const int stories = std::max(1, size.y);
	const int slotsPerStory = std::max(1, (N + stories - 1) / stories);

	// Concatenated band sheet (loader: simtower/animpeple/band = the seven
	// 0x85E8..0x85EE strips joined in id order, 99 frames of 16x24): the
	// EXE frame codes 0x00..0x62 index it directly.
	sf::Texture *bandTex = (bandCount > 0) ? &app->bitmaps["simtower/animpeple/band"] : NULL;
	const bool haveBandTex = bandTex && (int)bandTex->getSize().x >= 0x63 * 16;
	const int marginPx = (prototype->icon == ICON_LOBBY) ? 16 : 4;
	const float minX = (float)(position.x * 8 + marginPx);
	const float maxX = (float)(position.x * 8 + itemWidthPx - marginPx - frameW);

	for (int idx = 0; idx < N; ++idx)
	{
		Person * p = (bandCount > 0 ? NULL : peopleArr[idx]);

		int frame = 0;
		float slotJitter = 0.f;
		bool genericPeopleSheet = true;
		float px = 0.f;
		int story = 0;
		sf::Texture *peopleTex = NULL;

		if (bandCount > 0)
		{
			if (haveBandTex)
			{
				// Direct: the band code indexes the concatenated sheet.
				peopleTex = bandTex;
				frame = bandFrame[idx];
				genericPeopleSheet = false;
			}
			else
			{
				// Fallback: resolve the band frame code into its segment sheet.
				const char *sheet = NULL;
				for (unsigned b = 0; b < sizeof(kAnimPepleBand) / sizeof(kAnimPepleBand[0]); b++)
				{
					if (bandFrame[idx] >= kAnimPepleBand[b].first && bandFrame[idx] < kAnimPepleBand[b].first + kAnimPepleBand[b].count)
					{
						sheet = kAnimPepleBand[b].sheet;
						frame = bandFrame[idx] - kAnimPepleBand[b].first;
						break;
					}
				}
				if (sheet)
				{
					peopleTex = &app->bitmaps[sheet];
					// Placeholder fallback (extraction not regenerated):
					// use the generic masked people sheet instead.
					genericPeopleSheet = (peopleTex->getSize().x > 1024);
					if (genericPeopleSheet) peopleTex = &app->bitmaps["simtower/people"];
				}
			}
			// Band x is in cells from the unit's left edge; figures stand
			// on the floor line in the lower 24px of the 36px cell.
			px = (float)((position.x + bandX[idx]) * 8);
		}
		else
		{
			const char *sheet = "simtower/people";
			if (p->type == Person::kHousekeeper || id == "housekeeping") {
				sheet = "simtower/animpeple/housekeeper";
			} else if (p->type == Person::kSecurity || id == "security") {
				sheet = "simtower/animpeple/guard";
			} else if (id == "cinema" || id == "partyhall") {
				sheet = "simtower/animpeple/event";
			}
			genericPeopleSheet = (std::string(sheet) == "simtower/people");
			peopleTex = &app->bitmaps[sheet];
			// Placeholder fallback (extraction not regenerated): use the
			// generic masked people sheet rather than raw unhandled BMPs.
			if (peopleTex->getSize().x > 1024) {
				peopleTex = &app->bitmaps["simtower/people"];
				genericPeopleSheet = true;
			}
		}
		if (!peopleTex) peopleTex = &app->bitmaps["simtower/people"];
		int frameCount = std::max(1u, peopleTex->getSize().x / frameW);

		if (bandCount == 0)
		{
			if (genericPeopleSheet)
			{
				// Generic walk sheet: 6-frame continuous walking cycle.
				frame = (int)std::fmod(game->time.absolute * 100.0 + p->animOffset * frameCount, (double)frameCount);
			}
			else
			{
				// Multi-activity paired sheets (guard, housekeeper): hold
				// each activity for several seconds with a ~1-2 Hz micro-
				// animation toggle.
				const double phase = std::fmod((p->animOffset != 0.0 ? p->animOffset : idx * 0.25) * 17.0 + idx * 2.37, 1000.0);
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

			story = idx % stories;
			const int slot = idx / stories;
			px = (float)(position.x * 8 + (slot + 0.5f) * itemWidthPx / slotsPerStory - frameW / 2.0f) + slotJitter;
			if (maxX >= minX) {
				px = std::max(minX, std::min(maxX, px));
			} else {
				px = (float)(position.x * 8 + (itemWidthPx - frameW) / 2.0f);
			}
		}
		frame = ((frame % frameCount) + frameCount) % frameCount;

		const float py = (float)(-(position.y + story) * 36);

		sf::Sprite ps(*peopleTex);
		ps.setTextureRect(sf::IntRect({frame * frameW, 0}, {frameW, frameH}));
		ps.setOrigin({0.f, (float)frameH});
		ps.setPosition({px, py});

		sf::Color color = sf::Color::White;
		if (genericPeopleSheet) {
			color = sf::Color::Black;
			if (p) {
				if (p->stress > 80) color = sf::Color(255, 0, 0);
				else if (p->stress > 40) color = sf::Color(255, 128, 128);
			}
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
