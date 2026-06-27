/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Phase 3.3 - Emergency events: fire and terror (bomb threat).
 * Mirrors EventT.h/c, FireT.h/c from the original SimTower (codemap.md:833-862, 958-990).
 * Art resources are loaded by SimTowerLoader (fire/large, fire/small, fire/chopper,
 * fire/destroyed, alerts/fire, alerts/terrorist) and were previously unused.
 */
#include "EventSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "Logger.h"
#include "Person.h"
#include "Time.h"
#include "TimeWindow.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace OT;

namespace
{
	// --- Scheduling tunables ---
	const int    kMinRatingForEvent  = 1;    // 2-star tower minimum
	const double kEventGapMinDays    = 30.0; // ~4 in-game weeks
	const double kEventGapMaxDays    = 90.0; // ~3 in-game months
	// First event comes a bit sooner so the feature is discoverable.
	const double kFirstEventGapDays  = 15.0;

	// --- Fire tunables (in absolute-time units via Time::hourToAbsolute) ---
	const double kFireSpreadHours     = 0.25; // spread every ~15 game-minutes
	const double kHelicopterArriveHrs = 0.5;  // ~30 game-min to arrive
	const int    kHelicopterCost      = 10000;
	const int    kMaxBurningTiles     = 12;
	const double kFireStressPerSec    = 8.0;  // stress applied per real second

	// --- Terror tunables ---
	const double kBombCountdownHours  = 2.0;  // player has ~2 game-hours to defuse
	const int    kBombDamageCost      = 80000;
	const int    kBombDamageRadius    = 4;    // tiles
	const double kBombStressHit       = 40.0;

	double randDays(double lo, double hi)
	{
		const double r = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
		return lo + r * (hi - lo);
	}
}

EventSystem::EventSystem(Game * game)
: GameObject(game)
{
	reset();

	// Load the art. These textures were already extracted by SimTowerLoader
	// but never referenced anywhere — wire them up now.
	fireLargeSprite.SetImage(App->bitmaps["simtower/fire/large"]);
	fireLargeSprite.setTextureRect(sf::IntRect({0, 0}, {96, 36}));
	fireLargeSprite.setOrigin({48.f, 36.f});

	chopperSprite.SetImage(App->bitmaps["simtower/fire/chopper"]);
	chopperSprite.setOrigin({48.f, 36.f});

	destroyedSprite.SetImage(App->bitmaps["simtower/fire/destroyed"]);
	destroyedSprite.setOrigin({124.f, 24.f});

	alertSprite.SetImage(App->bitmaps["simtower/alerts/fire"]);
	alertSprite.setOrigin({38.f, 60.f});
}

void EventSystem::reset()
{
	type = Type::None;
	nextEventTime = 0;
	burningTiles.clear();
	fireOrigin = int2(0, 0);
	fireSpreadTimer = 0;
	fireAnimTime = 0;
	helicopterCalled = false;
	helicopterArriveTime = 0;
	destroyedTiles.clear();
	bombLocation = int2(0, 0);
	bombDetonateTime = 0;
	defused = false;
}

void EventSystem::scheduleNextEvent()
{
	if (game->rating < kMinRatingForEvent) { nextEventTime = 0; return; }

	// Don't schedule if the tower is basically empty.
	if (game->items.empty()) { nextEventTime = 0; return; }

	double gapDays = (nextEventTime == 0)
		? kFirstEventGapDays
		: randDays(kEventGapMinDays, kEventGapMaxDays);
	nextEventTime = game->time.absolute + Time::hourToAbsolute(24.0 * gapDays);
}

int2 EventSystem::pickRandomTenantTile() const
{
	// Collect candidate items that have a position and are real tenants
	// (not floors/lobbies/elevators which are structural).
	std::vector<Item::Item *> candidates;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); i++) {
		Item::Item * item = *i;
		if (item->underConstruction) continue;
		const std::string & id = item->prototype->id;
		if (id == "floor" || id == "lobby") continue;
		if (id.find("elevator") != std::string::npos) continue;
		if (id == "stairs" || id == "escalator") continue;
		candidates.push_back(item);
	}
	if (candidates.empty()) return int2(0, 0);

	Item::Item * picked = candidates[rand() % candidates.size()];
	// Pick a tile within the item's footprint.
	int x = picked->position.x + (picked->size.x > 1 ? rand() % picked->size.x : 0);
	int y = picked->position.y;
	return int2(x, y);
}

void EventSystem::startFire()
{
	type = Type::Fire;
	fireOrigin = pickRandomTenantTile();
	burningTiles.clear();
	burningTiles[fireOrigin.y].insert(fireOrigin.x);
	fireSpreadTimer = Time::hourToAbsolute(kFireSpreadHours);
	fireAnimTime = 0;
	helicopterCalled = false;
	helicopterArriveTime = 0;
	destroyedTiles.clear();

	char buf[128];
	snprintf(buf, sizeof(buf), "FIRE reported on floor %d! Click the blaze to send a helicopter.", fireOrigin.y);
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Fire started at floor %d x=%d", fireOrigin.y, fireOrigin.x);
}

void EventSystem::startTerror()
{
	type = Type::Terror;
	bombLocation = pickRandomTenantTile();
	bombDetonateTime = game->time.absolute + Time::hourToAbsolute(kBombCountdownHours);
	defused = false;

	alertSprite.SetImage(App->bitmaps["simtower/alerts/terrorist"]);
	alertSprite.setOrigin({38.f, 67.f});

	char buf[128];
	snprintf(buf, sizeof(buf), "BOMB THREAT on floor %d! Click the threat to defuse it!", bombLocation.y);
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Terror event started at floor %d x=%d", bombLocation.y, bombLocation.x);
}

void EventSystem::spreadFire()
{
	if (burningTiles.size() >= kMaxBurningTiles) return;

	// Gather all currently-burning tiles, then pick a random one and try
	// to spread to an adjacent tile.
	std::vector<std::pair<int, int>> burning;
	for (std::map<int, std::set<int>>::const_iterator fl = burningTiles.begin();
	     fl != burningTiles.end(); ++fl) {
		for (std::set<int>::const_iterator x = fl->second.begin();
		     x != fl->second.end(); ++x) {
			burning.push_back(std::make_pair(fl->first, *x));
		}
	}
	if (burning.empty()) return;

	std::pair<int, int> src = burning[rand() % burning.size()];
	// Candidate spread directions: left, right, up, down.
	int dx[] = {-1, 1, 0, 0};
	int dy[] = {0, 0, 1, -1};
	int dir = rand() % 4;
	int nx = src.second + dx[dir];
	int ny = src.first + dy[dir];

	// Only spread if there's something at that tile (check itemsByFloor).
	Game::ItemSetByInt::const_iterator flIt = game->itemsByFloor.find(ny);
	if (flIt == game->itemsByFloor.end()) return;
	bool occupied = false;
	for (Game::ItemSet::const_iterator i = flIt->second.begin();
	     i != flIt->second.end(); ++i) {
		recti r = (*i)->getRect();
		if (nx >= r.minX() && nx < r.maxX()) { occupied = true; break; }
	}
	if (!occupied) return;

	burningTiles[ny].insert(nx);
	LOG(DEBUG, "Fire spread to floor %d x=%d (%d tiles burning)", ny, nx,
	    (int)burningTiles.size());
}

void EventSystem::applyFireStress(double dt)
{
	// Increase stress for every person located at a burning tile's item.
	double stress = kFireStressPerSec * dt;
	for (std::map<int, std::set<int>>::const_iterator fl = burningTiles.begin();
	     fl != burningTiles.end(); ++fl) {
		Game::ItemSetByInt::const_iterator flIt = game->itemsByFloor.find(fl->first);
		if (flIt == game->itemsByFloor.end()) continue;
		for (Game::ItemSet::const_iterator i = flIt->second.begin();
		     i != flIt->second.end(); ++i) {
			Item::Item * item = *i;
			recti r = item->getRect();
			// Check if any burning x falls within this item.
			bool hit = false;
			for (std::set<int>::const_iterator x = fl->second.begin();
			     x != fl->second.end(); ++x) {
				if (*x >= r.minX() && *x < r.maxX()) { hit = true; break; }
			}
			if (!hit) continue;
			item->evaluation = std::max(0.0, item->evaluation - stress);
			for (Item::Item::People::iterator p = item->people.begin();
			     p != item->people.end(); ++p) {
				(*p)->addStress(stress);
			}
		}
	}
}

void EventSystem::endEvent()
{
	if (type == Type::Fire) {
		if (helicopterCalled) {
			game->timeWindow.showMessage("Fire extinguished. Tower is safe.");
		}
		// Mark heavily-burned tiles as destroyed for visual continuity.
		for (std::map<int, std::set<int>>::const_iterator fl = burningTiles.begin();
		     fl != burningTiles.end(); ++fl) {
			for (std::set<int>::const_iterator x = fl->second.begin();
			     x != fl->second.end(); ++x) {
				destroyedTiles.insert(encodeTile(fl->first, *x));
			}
		}
	}
	type = Type::None;
	burningTiles.clear();
	helicopterCalled = false;
	scheduleNextEvent();
}

void EventSystem::advance(double dt)
{
	// --- Scheduling ---
	if (type == Type::None) {
		if (nextEventTime == 0) {
			scheduleNextEvent();
		}
		if (nextEventTime > 0 && game->time.absolute >= nextEventTime) {
			// Roll the event type. Fire is more common than terror.
			if ((rand() % 100) < 70)
				startFire();
			else
				startTerror();
		}
		return;
	}

	// --- Active event progression ---
	fireAnimTime += dt;

	if (type == Type::Fire) {
		// Spread.
		fireSpreadTimer -= game->time.dta;
		if (fireSpreadTimer <= 0) {
			spreadFire();
			fireSpreadTimer = Time::hourToAbsolute(kFireSpreadHours);
		}
		// Stress.
		applyFireStress(dt);
		// Helicopter arrival.
		if (helicopterCalled && game->time.absolute >= helicopterArriveTime) {
			LOG(IMPORTANT, "Helicopter extinguished fire (%d tiles burned)",
			    (int)burningTiles.size());
			endEvent();
		}
	}
	else if (type == Type::Terror) {
		// Bomb countdown.
		if (game->time.absolute >= bombDetonateTime && !defused) {
			// Detonation.
			LOG(IMPORTANT, "Bomb detonated at floor %d x=%d", bombLocation.y, bombLocation.x);
			game->transferFunds(-kBombDamageCost, "emergency",
			                    "Bomb damage repairs");
			// Stress everyone nearby.
			Game::ItemSetByInt::const_iterator flIt = game->itemsByFloor.find(bombLocation.y);
			if (flIt != game->itemsByFloor.end()) {
				for (Game::ItemSet::const_iterator i = flIt->second.begin();
				     i != flIt->second.end(); ++i) {
					Item::Item * item = *i;
					recti r = item->getRect();
					int dist = std::min(abs(r.minX() - bombLocation.x),
					                    abs(r.maxX() - bombLocation.x));
					if (dist <= kBombDamageRadius) {
						item->evaluation = std::max(0.0, item->evaluation - 30.0);
						for (Item::Item::People::iterator p = item->people.begin();
						     p != item->people.end(); ++p) {
							(*p)->addStress(kBombStressHit);
						}
					}
				}
			}
			game->timeWindow.showMessage("BOMB DETONATED! Extensive damage reported.");
			endEvent();
		}
	}
}

bool EventSystem::handleClick(int2 tile)
{
	if (type == Type::Fire && !helicopterCalled) {
		// Accept a click anywhere within the burning area or near it.
		bool near = false;
		for (std::map<int, std::set<int>>::const_iterator fl = burningTiles.begin();
		     fl != burningTiles.end(); ++fl) {
			for (std::set<int>::const_iterator x = fl->second.begin();
			     x != fl->second.end(); ++x) {
				if (abs(tile.x - *x) <= 6 && abs(tile.y - fl->first) <= 2) {
					near = true; break;
				}
			}
			if (near) break;
		}
		if (near) {
			helicopterCalled = true;
			helicopterArriveTime = game->time.absolute + Time::hourToAbsolute(kHelicopterArriveHrs);
			game->transferFunds(-kHelicopterCost, "emergency", "Helicopter dispatch");
			game->timeWindow.showMessage("Helicopter dispatched! Extinguishing in progress...");
			alertSprite.SetImage(App->bitmaps["simtower/alerts/chopper"]);
			alertSprite.setOrigin({62.f, 67.f});
			LOG(IMPORTANT, "Helicopter dispatched for fire (cost $%d)", kHelicopterCost);
			return true;
		}
	}
	else if (type == Type::Terror && !defused) {
		// Defuse if the click is near the bomb.
		if (abs(tile.x - bombLocation.x) <= 6 && abs(tile.y - bombLocation.y) <= 2) {
			defused = true;
			game->timeWindow.showMessage("Bomb defused! Crisis averted.");
			LOG(IMPORTANT, "Bomb defused at floor %d x=%d", bombLocation.y, bombLocation.x);
			endEvent();
			return true;
		}
	}
	return false;
}

void EventSystem::draw(sf::RenderTarget & target, sf::RenderStates /*states*/) const
{
	if (type == Type::None) {
		// Draw ruins from previous fires that haven't been rebuilt over.
		// (Keeps the visual consequence of a past fire.)
		return;
	}

	if (type == Type::Fire) {
		// Fire animation: cycle 4 frames of 96px each from the 384px strip.
		int frame = static_cast<int>(fireAnimTime * 12.0) % 4;
		sf::IntRect fireRect({frame * 96, 0}, {96, 36});

		// Draw a fire sprite at each burning tile cluster. We render at the
		// fire origin for the main blaze plus smaller markers for spread tiles.
		Sprite f = fireLargeSprite;
		f.setTextureRect(fireRect);

		// Main blaze at origin.
		f.setPosition(tileToPixel(fireOrigin.x, fireOrigin.y));
		target.draw(f);
		game->drawnSprites++;

		// Additional fire markers for spread tiles (excluding origin).
		for (std::map<int, std::set<int>>::const_iterator fl = burningTiles.begin();
		     fl != burningTiles.end(); ++fl) {
			for (std::set<int>::const_iterator x = fl->second.begin();
			     x != fl->second.end(); ++x) {
				if (*x == fireOrigin.x && fl->first == fireOrigin.y) continue;
				f.setPosition(tileToPixel(*x, fl->first));
				target.draw(f);
				game->drawnSprites++;
			}
		}

		// Helicopter approaching.
		if (helicopterCalled) {
			float progress = 1.0f;
			double remaining = helicopterArriveTime - game->time.absolute;
			double total = Time::hourToAbsolute(kHelicopterArriveHrs);
			if (total > 0) progress = 1.0f - (float)(remaining / total);
			progress = std::max(0.0f, std::min(1.0f, progress));
			// Chopper sweeps in from the upper-left.
			sf::Vector2f dest = tileToPixel(fireOrigin.x, fireOrigin.y);
			Sprite chop = chopperSprite;
			chop.setPosition({dest.x - 200.0f * (1.0f - progress), dest.y - 80.0f * (1.0f - progress)});
			target.draw(chop);
			game->drawnSprites++;
		}

		// Alert icon above the blaze.
		if (!helicopterCalled) {
			Sprite a = alertSprite;
			a.SetImage(App->bitmaps["simtower/alerts/fire"]);
			a.setOrigin({38.f, 60.f});
			a.setPosition({tileToPixel(fireOrigin.x, fireOrigin.y).x,
			               tileToPixel(fireOrigin.x, fireOrigin.y).y - 40.f});
			target.draw(a);
			game->drawnSprites++;
		}
	}
	else if (type == Type::Terror) {
		// Blinking alert at the bomb location.
		bool blink = (static_cast<int>(fireAnimTime * 4.0) % 2) == 0;
		if (blink) {
			Sprite a = alertSprite;
			a.SetImage(App->bitmaps["simtower/alerts/terrorist"]);
			a.setOrigin({38.f, 67.f});
			a.setPosition({tileToPixel(bombLocation.x, bombLocation.y).x,
			               tileToPixel(bombLocation.x, bombLocation.y).y - 40.f});
			target.draw(a);
			game->drawnSprites++;
		}
	}
}

void EventSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	xml.PushAttribute("eventNextTime", nextEventTime);
	// Active events are not persisted mid-resolution; they end on reload.
	// This is intentional — a fire half-extinguished is simpler to just
	// clear than to faithfully restore the spread pattern.
}

void EventSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	nextEventTime = xml.DoubleAttribute("eventNextTime", 0.0);
	// If the save had an active event, it's gone on reload — the tower
	// is safe again. nextEventTime is preserved so the scheduling cadence
	// isn't reset.
	type = Type::None;
	burningTiles.clear();
}
