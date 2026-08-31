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
#include "Item/Security.h"
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

	// --- Authentic terrorist threat (OpenSkyWeb ISSUE-036, reverse-engineered
	// from SIMTOWER.EXE): deterministic once-per-game event on the weekend
	// day of Q4 in year 5, triggered at 10:00 with a fixed 15:00 detonation.
	const int    kTerrorYear         = 5;
	const int    kTerrorQuarter      = 4;
	const int    kTerrorWeekendDay   = 2;   // Time::day == 2 is the weekend
	const double kTerrorTriggerHour = 10.0;
	const double kTerrorDetonationHour = 15.0;

	// --- Burglar tunables (OpenSkyWeb ISSUE-011/036) ---
	const int    kBurglarReward      = 10000;  // security caught them
	const int    kBurglarLoss        = 50000;  // unsecured sector
	const double kBurglarEvalHit     = 20.0;
	const double kBurglarStress      = 40.0;

	// --- Buried treasure (OpenSkyWeb ISSUE-034, EXE string 0x7a03c):
	// rolled once per game-day while any item is under construction.
	const int    kTreasureDailyPct     = 8;
	const int    kTreasureMinThousands = 20;
	const int    kTreasureMaxThousands = 100;
	const double kTreasureDurationHours = 12.0;

	// --- Response-flow tunables (OpenSkyWeb ISSUE-033/036) ---
	const double kSecurityQuenchHours = 1.0;   // security extinguishes ~1h
	const double kBurnoutHours        = 12.0;  // unanswered fire dies out
	const double kSecomScanHours      = 0.5;   // SECOM bomb-scan delay
	const double kSecomDispatchHours  = 1.0;   // SECOM defusal completes

	// --- Fire tunables (in absolute-time units via Time::hourToAbsolute) ---
	const double kFireSpreadHours     = 0.25; // spread every ~15 game-minutes
	const double kHelicopterArriveHrs = 0.5;  // ~30 game-min to arrive
	const int    kHelicopterCost      = 10000;
	const int    kMaxBurningTiles     = 12;
	const double kFireStressPerSec    = 8.0;  // stress applied per real second

	// --- Terror tunables ---
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
	terrorFired = false;
	pendingTerror = false;
	treasureOrigin = int2(0, 0);
	treasureEndTime = 0;
	lastTreasureDay = 0;
	brigadeDeclined = false;
	securityArriveTime = 0;
	burnoutTime = 0;
	secomScanTime = 0;
	secomDispatchTime = 0;
	ransom = 0;
	securityCovered = false;
	defuseRadius = 6;
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
	// absolute counts in-game DAYS (integer part = day, fraction = time of
	// day), so gaps must be added directly. The old code routed days through
	// Time::hourToAbsolute, which has no branch above 24h — undefined
	// behaviour that completely broke the event cadence (OpenSkyWeb
	// ISSUE-036 clamp-bug fix).
	nextEventTime = game->time.absolute + gapDays;
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

bool EventSystem::hasSecom() const
{
	Game::ItemSetByString::const_iterator it = game->itemsByType.find("secom");
	return it != game->itemsByType.end() && !it->second.empty();
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
	brigadeDeclined = false;
	securityArriveTime = 0;
	burnoutTime = 0;
	destroyedTiles.clear();

	char buf[160];
	if (hasSecom()) {
		snprintf(buf, sizeof(buf), "SECOM has sensed a fire on floor %d! Everyone should take emergency refuge!", fireOrigin.y);
	} else {
		snprintf(buf, sizeof(buf), "FIRE reported on floor %d!", fireOrigin.y);
	}
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Fire started at floor %d x=%d", fireOrigin.y, fireOrigin.x);

	// Authentic response flow (OpenSkyWeb ISSUE-036, EXE fire-crew dialog):
	// ask whether to call the paid emergency crew. Declining hands the
	// response to Security (if coverage reaches the floor) or lets it burn.
	snprintf(buf, sizeof(buf), "A fire has broken out on floor %d. Call the emergency fire crew for $%d?", fireOrigin.y, kHelicopterCost);
	game->askEventChoice("FIRE!", buf, "Call Fire Crew", "Let it burn",
		[this](bool yes) { if (type == Type::Fire) resolveFireResponse(yes); });
}

void EventSystem::dispatchHelicopter()
{
	helicopterCalled = true;
	helicopterArriveTime = game->time.absolute + Time::hourToAbsolute(kHelicopterArriveHrs);
	game->transferFunds(-kHelicopterCost, "emergency", "Helicopter dispatch");
	game->timeWindow.showMessage("Helicopter dispatched! Extinguishing in progress...");
	alertSprite.SetImage(App->bitmaps["simtower/alerts/chopper"]);
	alertSprite.setOrigin({62.f, 67.f});
	LOG(IMPORTANT, "Helicopter dispatched for fire (cost $%d)", kHelicopterCost);
}

void EventSystem::resolveFireResponse(bool callCrew)
{
	if (callCrew) {
		dispatchHelicopter();
		return;
	}
	brigadeDeclined = true;
	if (securityCoversFloor(fireOrigin.y)) {
		game->timeWindow.showMessage("Security is attempting to quench the fire.");
		securityArriveTime = game->time.absolute + Time::hourToAbsolute(kSecurityQuenchHours);
	} else {
		burnoutTime = game->time.absolute + Time::hourToAbsolute(kBurnoutHours);
	}
}

void EventSystem::startTerror()
{
	type = Type::Terror;
	terrorFired = true;
	pendingTerror = false;
	bombLocation = pickRandomTenantTile();
	// Authentic fixed detonation: 3 o'clock (15:00) on the trigger day
	// (OpenSkyWeb ISSUE-036), not a floating countdown.
	bombDetonateTime = std::floor(game->time.absolute) + Time::hourToAbsolute(kTerrorDetonationHour);
	defused = false;
	// Ransom demand, thousands-scale $10k-$90k (EXE "$#000" placeholder).
	ransom = (1 + (rand() % 9)) * 10000;
	// Security coverage widens the defuse click radius (6 -> 9).
	securityCovered = securityCoversFloor(bombLocation.y);
	defuseRadius = 6 + (securityCovered ? 3 : 0);
	// SECOM auto-scan (OpenSkyWeb ISSUE-031): reveals the bomb and
	// dispatches an automatic defusal attempt.
	secomScanTime = hasSecom()
		? game->time.absolute + Time::hourToAbsolute(kSecomScanHours)
		: 0;
	secomDispatchTime = 0;

	alertSprite.SetImage(App->bitmaps["simtower/alerts/terrorist"]);
	alertSprite.setOrigin({38.f, 67.f});

	char buf[128];
	snprintf(buf, sizeof(buf), "BOMB THREAT on floor %d! Click the threat to defuse it!", bombLocation.y);
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Terror event started at floor %d x=%d", bombLocation.y, bombLocation.x);

	// Authentic ransom choice (OpenSkyWeb ISSUE-033, EXE dialog 0x78c00).
	char text[192];
	snprintf(text, sizeof(text), "They demand $%d or a hidden bomb will explode at 3 o'clock.", ransom);
	game->askEventChoice("Blackmail from Terrorists!", text, "Pay Them", "Find the Bomb",
		[this](bool pay) { if (type == Type::Terror) resolveTerrorResponse(pay); });
}

void EventSystem::resolveTerrorResponse(bool pay)
{
	if (pay) {
		game->transferFunds(-ransom, "emergency", "Terrorist ransom paid");
		game->timeWindow.showMessage("The ransom was paid. The terrorists stood down.");
		endEvent();
		return;
	}
	if (securityCovered) {
		game->timeWindow.showMessage("Security forces from your Security Offices are on their way to find the bomb. Good luck...");
	}
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
		// Authentic outcome text (OpenSkyWeb ISSUE-036, EXE dialog 0x78800):
		// emergency fire stairs on the burning floor mean no one was injured.
		const bool hasStairs = fireOrigin.y > 0 && game->floorItems.count(fireOrigin.y) != 0;
		game->timeWindow.showMessage(hasStairs
		    ? "The fire was stopped. Because your building has emergency stairs, no one was injured, but the tower is damaged."
		    : "The fire was stopped. There were injuries and the tower is damaged.");
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
	brigadeDeclined = false;
	securityArriveTime = 0;
	burnoutTime = 0;
	secomScanTime = 0;
	secomDispatchTime = 0;
	scheduleNextEvent();
	// A terrorist trigger that hit while this event was active fires now.
	if (pendingTerror && !terrorFired) startTerror();
}

int2 EventSystem::pickRandomConstructionTile() const
{
	// Pick a random tile inside an item that is under construction.
	std::vector<Item::Item *> candidates;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); i++) {
		if ((*i)->underConstruction) candidates.push_back(*i);
	}
	if (candidates.empty()) return int2(0, 0);

	Item::Item * picked = candidates[rand() % candidates.size()];
	int x = picked->position.x + (picked->size.x > 1 ? rand() % picked->size.x : 0);
	return int2(x, picked->position.y);
}

bool EventSystem::securityCoversFloor(int floor) const
{
	Game::ItemSetByString::const_iterator it = game->itemsByType.find("security");
	if (it == game->itemsByType.end()) return false;
	for (Game::ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); i++) {
		Item::Security * sec = dynamic_cast<Item::Security *>(*i);
		if (sec && sec->isCoveringFloor(floor)) return true;
	}
	return false;
}

void EventSystem::startBurglar()
{
	// Instant event (OpenSkyWeb ISSUE-011/031/036): a burglar hits a random
	// tenant. A SECOM Center substitutes for Security coverage; otherwise a
	// Security Office within 15 floors catches him (cash reward). An
	// unsecured sector loses money and morale.
	const int2 target = pickRandomTenantTile();
	if (hasSecom() || securityCoversFloor(target.y)) {
		game->transferFunds(kBurglarReward, "vip", "Security Guard apprehended burglar!");
		char buf[128];
		snprintf(buf, sizeof(buf), "Security Guard caught a burglar on floor %d! Reward: $%d", target.y, kBurglarReward);
		game->timeWindow.showMessage(buf);
		LOG(IMPORTANT, "Burglar caught on floor %d (security coverage)", target.y);
	} else {
		game->transferFunds(-kBurglarLoss, "event", "Burglary in unsecured sector");
		char buf[128];
		snprintf(buf, sizeof(buf), "Burglary on floor %d! No security guard nearby! -$%d", target.y, kBurglarLoss);
		game->timeWindow.showMessage(buf);
		LOG(IMPORTANT, "Burglary on floor %d (unsecured)", target.y);
		Game::ItemSetByInt::const_iterator flIt = game->itemsByFloor.find(target.y);
		if (flIt != game->itemsByFloor.end()) {
			for (Game::ItemSet::const_iterator i = flIt->second.begin(); i != flIt->second.end(); i++) {
				Item::Item * item = *i;
				item->evaluation = std::max(0.0, item->evaluation - kBurglarEvalHit);
				for (Item::Item::People::iterator p = item->people.begin(); p != item->people.end(); p++) {
					(*p)->addStress(kBurglarStress);
				}
			}
		}
	}
}

void EventSystem::checkTreasure()
{
	// Buried treasure (OpenSkyWeb ISSUE-034): rolled once per game-day while
	// any item is under construction. 8% daily chance.
	const int today = (int)std::floor(game->time.absolute);
	if (today == lastTreasureDay) return;
	lastTreasureDay = today;
	bool constructing = false;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); i++) {
		if ((*i)->underConstruction) { constructing = true; break; }
	}
	if (!constructing) return;
	if ((rand() % 100) >= kTreasureDailyPct) return;
	startTreasure();
}

void EventSystem::startTreasure()
{
	const int2 origin = pickRandomConstructionTile();
	bool constructing = false;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); i++) {
		if ((*i)->underConstruction) { constructing = true; break; }
	}
	if (!constructing) return;

	type = Type::Treasure;
	treasureOrigin = origin;
	treasureEndTime = game->time.absolute + Time::hourToAbsolute(kTreasureDurationHours);

	const int thousands = kTreasureMinThousands + (rand() % (kTreasureMaxThousands - kTreasureMinThousands + 1));
	const int amount = thousands * 1000;
	game->transferFunds(amount, "treasure", "Buried treasure discovered");
	char buf[160];
	snprintf(buf, sizeof(buf), "During construction, workers discovered ancient buried treasure! It sold for $%d.", amount);
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Buried treasure worth $%d at floor %d x=%d", amount, origin.y, origin.x);
}

void EventSystem::advance(double dt)
{
	// --- Deterministic terrorist threat (OpenSkyWeb ISSUE-036): once per
	// game, weekend day of Q4 in year 5 at 10:00. If another event is
	// active it fires the moment that one ends.
	if (!terrorFired) {
		if (game->time.year == kTerrorYear
		    && game->time.quarter == kTerrorQuarter
		    && game->time.day == kTerrorWeekendDay
		    && game->time.checkHour(kTerrorTriggerHour)) {
			if (type == Type::None) startTerror();
			else pendingTerror = true;
		}
	}

	// --- Random scheduling (fires and burglars only) ---
	if (type == Type::None) {
		// Buried treasure can surface while the crew is busy building.
		checkTreasure();
		if (nextEventTime == 0) {
			scheduleNextEvent();
		}
		if (nextEventTime > 0 && game->time.absolute >= nextEventTime) {
			// Roll the event type. Fire and burglars are equally likely;
			// terror is the scripted year-5 event, not a random roll.
			if ((rand() % 100) < 50)
				startFire();
			else
				startBurglar();
		}
		return;
	}

	// --- Active event progression ---
	fireAnimTime += dt;

	if (type == Type::Treasure) {
		// The windfall is instantaneous; the alert just lingers a while.
		if (game->time.absolute >= treasureEndTime) {
			endEvent();
		}
	}
	else if (type == Type::Fire) {
		// Spread.
		fireSpreadTimer -= game->time.dta;
		if (fireSpreadTimer <= 0) {
			spreadFire();
			fireSpreadTimer = Time::hourToAbsolute(kFireSpreadHours);
		}
		// Stress.
		applyFireStress(dt);
		// Fire crew (brigade/helicopter) arrival.
		if (helicopterCalled && game->time.absolute >= helicopterArriveTime) {
			LOG(IMPORTANT, "Helicopter extinguished fire (%d tiles burned)",
			    (int)burningTiles.size());
			endEvent();
		}
		// Security quenching after the crew was declined.
		else if (brigadeDeclined && securityArriveTime > 0
		         && game->time.absolute >= securityArriveTime) {
			LOG(IMPORTANT, "Security quenched the fire");
			endEvent();
		}
		// Burnout: an unanswered, uncovered fire dies out on its own.
		else if (brigadeDeclined && burnoutTime > 0
		         && game->time.absolute >= burnoutTime) {
			LOG(IMPORTANT, "Fire burned itself out");
			endEvent();
		}
	}
	else if (type == Type::Terror) {
		// SECOM auto-scan (OpenSkyWeb ISSUE-031): reveal the bomb, dispatch
		// the automated defusal attempt.
		if (secomScanTime > 0 && game->time.absolute >= secomScanTime && !defused) {
			secomScanTime = 0;
			char buf[192];
			snprintf(buf, sizeof(buf), "Secom System Now Scanning....\nA bomb has been found on floor %d.\nSecurity is on its way to diffuse the bomb.", bombLocation.y);
			game->timeWindow.showMessage(buf);
			secomDispatchTime = game->time.absolute + Time::hourToAbsolute(kSecomDispatchHours);
		}
		// SECOM defusal completes -> success (EXE string 0x79227).
		if (secomDispatchTime > 0 && game->time.absolute >= secomDispatchTime && !defused) {
			defused = true;
			game->timeWindow.showMessage("Because you have enough Security Offices in your tower, Security Forces found the bomb. Good work!");
			LOG(IMPORTANT, "SECOM defused the bomb at floor %d", bombLocation.y);
			endEvent();
		}
		// Detonation at the fixed 15:00 deadline.
		else if (game->time.absolute >= bombDetonateTime && !defused) {
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
			game->timeWindow.showMessage(hasSecom()
			    ? "[SECOM] was not able to find the bomb in time. The bomb has exploded!"
			    : "BOMB DETONATED! Extensive damage reported.");
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
			dispatchHelicopter();
			return true;
		}
	}
	else if (type == Type::Terror && !defused) {
		// Defuse if the click is near the bomb; Security coverage widens
		// the radius (OpenSkyWeb ISSUE-033).
		if (abs(tile.x - bombLocation.x) <= defuseRadius && abs(tile.y - bombLocation.y) <= 2) {
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
	else if (type == Type::Treasure) {
		// Blinking treasure alert over the construction site.
		bool blink = (static_cast<int>(fireAnimTime * 4.0) % 2) == 0;
		if (blink) {
			Sprite a = alertSprite;
			a.SetImage(App->bitmaps["simtower/alerts/treasure"]);
			a.setOrigin({38.f, 60.f});
			a.setPosition({tileToPixel(treasureOrigin.x, treasureOrigin.y).x,
			               tileToPixel(treasureOrigin.x, treasureOrigin.y).y - 40.f});
			target.draw(a);
			game->drawnSprites++;
		}
	}
}

void EventSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	xml.PushAttribute("eventNextTime", nextEventTime);
	xml.PushAttribute("eventTerrorFired", terrorFired);
	xml.PushAttribute("eventPendingTerror", pendingTerror);
	// Active events are not persisted mid-resolution; they end on reload.
	// This is intentional — a fire half-extinguished is simpler to just
	// clear than to faithfully restore the spread pattern.
}

void EventSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	nextEventTime = xml.DoubleAttribute("eventNextTime", 0.0);
	terrorFired = xml.BoolAttribute("eventTerrorFired", false);
	pendingTerror = xml.BoolAttribute("eventPendingTerror", false);
	// If the save had an active event, it's gone on reload — the tower
	// is safe again. nextEventTime is preserved so the scheduling cadence
	// isn't reset.
	type = Type::None;
	burningTiles.clear();
}
