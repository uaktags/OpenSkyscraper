/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Scheduled disasters (TODO.md "Concilia-tower adoption review (2026-09-14)"
 * rows 12-18): deterministic calendar, EXE-model fire fronts + helicopter +
 * foot guards, bomb threat + guard hunt + blast, buried treasure, Santa.
 * Re-expressed in our C++ idiom from concilia-tower's clean-room SIMTOWER
 * decode (EventT seg_10c8, FireT seg_10e8, TimeT seg_1208, SantaT seg_11b8,
 * LevelT seg_1148). Constants are byte-verified; see local://ground-truth.md.
 */
#include "EventSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Cathedral.h"
#include "Item/Item.h"
#include "Logger.h"
#include "Person.h"
#include "Time.h"
#include "TimeWindow.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace OT;

namespace
{
	// --- Calendar (TimeT 1208:0342) ---
	const int kFirePeriodDays  = 84;   // day % 0x54 == 0x53
	const int kBombPeriodDays  = 60;   // day % 0x3C == 0x3B
	const int kSantaPeriodDays = 12;   // day % 0xC == 0xB, launched at 21:00
	const int kDispatchHour    = 10;   // the 10:00 disaster dispatch

	// --- Fire (FireT seg_10e8, tuning 0x7F05) ---
	const int kFireStarGate   = 2;       // StartFire: star > 2
	const int kFireMinExtent  = 32;      // StartFire: extent >= 0x20
	const int kFireCells      = 12;      // FIRE_FRONT_CELLS: one 96px strip
	const int kFireSpreadFrames = 7;     // 0xDDD0: 1 cell / 7 frames
	const int kFireFloorFrames  = 80;    // 0xDDD2: +80 frames per floor up
	const int kFireChopperCost  = 500000; // 0xDE14 = 5000, x$100
	const int kFireEndFrame     = 2000;  // hard stop at frame 2000 = 21:00

	// --- Bomb (EventT seg_10c8, tuning 0xDE1C/1E/20) ---
	const int kBombRansom[3] = { 200000, 300000, 1000000 }; // stars 2/3/4
	const int kBombBlastFloorsDown = 2;  // DestroyTenants 10c8:02bd
	const int kBombBlastFloorsUp   = 3;
	const int kBombBlastHalfCells  = 20;
	const int kBombDetonateFrame   = 1200; // EXE ft 0x4B0 = 13:00

	// --- Guards (GuardT seg_10f8) ---
	const int kGuardsPerOffice = 6;
	const int kGuardSweepFrames = 2; // 1 cell / 2 frames (linger 0xDDCC)
	const int kGuardFloorFrames = 3; // transit frames per floor (0xDDCE)
	const int kGuardDouseFrames = 5; // fire-mode action pause (0xDDDA)

	// --- Buried treasure (LevelT seg_1148, tuning 0xA8/AA/AC) ---
	const int kTreasureAwards[3] = { 200000, 300000, 500000 };
	const int kTreasureExtentPerStar = 25; // extent > star * 0x19
	const int kCashCap = 99999999;         // AwardMoney 1178:076f clamp

	// --- Santa (SantaT seg_11b8) ---
	const int kSantaTickPixels = 10; // x -= 10 px/tick
	const float kSantaBaseY = -60.0f * 36.0f; // world altitude ~0x84c px up (floor ~60)

	// Event cleanup: EXE ft 0x5DC = 1500 = 16:00. ExtinguishFire
	// (10f0:02ec) jumps only if earlier; bomb resolution (10d0:02ab)
	// writes this frame unconditionally.
	const int kCleanupFrame = 1500;

	// Guard sweep range (concilia TOWER_MIN_FLOOR / TOWER_TOP_FLOOR).
	const int kGuardMinFloor = -20;
	const int kGuardMaxFloor = 100;
}

EventSystem::EventSystem(Game * game)
: GameObject(game)
{
	reset();

	fireSprite.SetImage(App->bitmaps["simtower/fire/large"]);
	fireSprite.setTextureRect(sf::IntRect({0, 0}, {96, 36}));
	fireSprite.setOrigin({48.f, 36.f});

	smallFireSprite.SetImage(App->bitmaps["simtower/fire/small"]);
	smallFireSprite.setTextureRect(sf::IntRect({0, 0}, {96, 36}));
	smallFireSprite.setOrigin({48.f, 36.f});

	chopperSprite.SetImage(App->bitmaps["simtower/fire/chopper"]);
	chopperSprite.setTextureRect(sf::IntRect({0, 0}, {96, 36})); // 4 x 96px rotor frames
	chopperSprite.setOrigin({48.f, 36.f});

	alertSprite.SetImage(App->bitmaps["simtower/alerts/fire"]);
	alertSprite.setOrigin({38.f, 60.f});

	// Burned cells render as rubble (resource 0x8FA8). The loader slice
	// owns the exact key; BitmapManager falls back to a placeholder.
	rubbleSprite.SetImage(App->bitmaps["simtower/fire/rubble"]);
	rubbleSprite.setOrigin({124.f, 24.f});

	santaSprite.SetImage(App->bitmaps["simtower/deco/santa"]);
	santaSprite.setOrigin({24.f, 24.f});
}

void EventSystem::reset()
{
	type = Type::None;
	prevAbsolute = game ? game->time.absolute : 0.0;
	lastDispatchDay = -1;
	santaActive = false;
	santaX = 0;
	santaY = 0;

	fireLeft.clear();
	fireRight.clear();
	fireOriginFloor = 0;
	fireOriginCell = 0;
	fireFrame = 0;
	fireFrameAccum = 0;
	fireChopperPending = false;
	fireChopperPaid = false;
	chopperX = 0;
	fireGuardsActive = false;
	guardOffices.clear();

	bombFloor = 0;
	bombSlot = 0;
	bombRansom = 0;
	bombArmed = false;
	bombCaught = false;

	treasureLatched = false;
	pendingBuilds.clear();
	rubble.clear();
}

// ---------------------------------------------------------------- calendar

void EventSystem::dailyDispatch()
{
	// TimeT @0357 before @0373: the fire check runs first, and a started
	// fire gates the bomb offer.
	const int day = (int)std::floor(game->time.absolute);
	if (day == lastDispatchDay) return;
	lastDispatchDay = day;

	if (day % kFirePeriodDays == kFirePeriodDays - 1)
		startFire();
	if (day % kBombPeriodDays == kBombPeriodDays - 1)
		offerBomb();
}

void EventSystem::launchSanta()
{
	santaActive = true;
	santaX = 4000.f; // off-screen right
	santaY = kSantaBaseY;
	LOG(IMPORTANT, "Santa flies across the tower");
}

// -------------------------------------------------------------------- fire

bool EventSystem::hasSecurity() const
{
	Game::ItemSetByString::const_iterator it = game->itemsByType.find("security");
	return it != game->itemsByType.end() && !it->second.empty();
}

bool EventSystem::floorExtent(int floor, int & left, int & right) const
{
	Game::ItemSetByInt::const_iterator it = game->itemsByFloor.find(floor);
	if (it == game->itemsByFloor.end() || it->second.empty()) return false;
	left = INT_MAX;
	right = INT_MIN;
	for (Game::ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i) {
		recti r = (*i)->getRect();
		left  = std::min(left, r.minX());
		right = std::max(right, r.maxX());
	}
	return right > left;
}

int EventSystem::topBuiltFloor() const
{
	int top = INT_MIN;
	for (Game::ItemSetByInt::const_iterator ii = game->itemsByFloor.begin();
	     ii != game->itemsByFloor.end(); ++ii) {
		if (ii->second.empty()) continue;
		if (ii->first > top) top = ii->first;
	}
	return top;
}

int EventSystem::lobbyHeight() const
{
	return game->mainLobby ? game->mainLobby->size.y : 1;
}

int EventSystem::pickDisasterFloor() const
{
	const int lo = lobbyHeight() + 1;
	const int top = topBuiltFloor();
	if (top < lo) return INT_MIN;
	return lo + rand() % (top - lo + 1);
}

void EventSystem::addRubble(int floor, int x)
{
	rubble.insert(encodeTile(floor, x));
}

bool EventSystem::fireBurning() const
{
	return !fireLeft.empty() || !fireRight.empty();
}

bool EventSystem::fireFloorIgniting(int floor) const
{
	std::map<int, int>::const_iterator l = fireLeft.find(floor);
	std::map<int, int>::const_iterator r = fireRight.find(floor);
	return l != fireLeft.end() && r != fireRight.end() && r->second - l->second < kFireCells;
}

void EventSystem::startFire()
{
	if (type != Type::None) return;
	const int star = game->rating + 1; // rating is 0-based (0 = 1 star)
	if (star <= kFireStarGate) return;                // star > 2
	if (!hasSecurity()) return;                          // a security office exists
	if (Item::Cathedral::towerHasCathedral(game)) return; // fires retire for good

	const int floor = pickDisasterFloor();
	if (floor == INT_MIN) return;
	int L, R;
	if (!floorExtent(floor, L, R)) return;
	if (R - L < kFireMinExtent) return; // StartFire: width must exceed 0x1F

	type = Type::Fire;
	fireOriginFloor = floor;
	fireOriginCell  = R - kFireMinExtent; // 32 cells in from the right edge
	fireLeft.clear();
	fireRight.clear();
	fireLeft[floor] = fireRight[floor] = fireOriginCell;
	fireFrame = 0;
	fireFrameAccum = 0;
	fireChopperPending = false;
	fireChopperPaid = false;
	chopperX = 0;
	fireGuardsActive = false;
	guardOffices.clear();

	char buf[160];
	snprintf(buf, sizeof(buf), "FIRE reported on floor %d!", floor);
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Fire started on floor %d at cell %d", floor, fireOriginCell);

	offerFireChopper();
}

void EventSystem::offerFireChopper()
{
	// 10e8:0147 sets the pending bit and pops the fire-crew dialog.
	fireChopperPending = true;
	char buf[192];
	snprintf(buf, sizeof(buf),
	         "A fire has broken out on floor %d. Call the fire helicopter for $%d?",
	         fireOriginFloor, kFireChopperCost);
	game->askEventChoice("FIRE!", buf, "Call Fire Crew", "Let it burn",
		[this](bool yes) { if (type == Type::Fire) resolveFireResponse(yes); });
}

void EventSystem::resolveFireResponse(bool callCrew)
{
	if (type != Type::Fire) return;
	fireChopperPending = false;
	if (callCrew) {
		callChopper();
		return;
	}
	// The free branch is the FOOT RESPONSE (10e8:017c): every security
	// office's six guards fight the fire on foot.
	game->timeWindow.showMessage("Security guards are fighting the fire on foot.");
	deployFireGuards();
}

void EventSystem::callChopper()
{
	if (type != Type::Fire) return;
	if (fireChopperPaid || chopperX > 0) return;
	if (game->funds < kFireChopperCost) {
		game->timeWindow.showMessage("You cannot afford the fire helicopter.");
		return;
	}
	int L, R;
	if (!floorExtent(fireOriginFloor, L, R)) R = fireOriginCell + kFireCells;
	// The chopper starts at the origin floor's right extent - 12 and
	// sweeps left (10e8:0147/0450).
	chopperX = R - kFireCells;
	fireChopperPaid = true;
	game->transferFunds(-kFireChopperCost, "emergency", "Fire helicopter dispatched");
	LOG(IMPORTANT, "Fire helicopter dispatched ($%d)", kFireChopperCost);
}

void EventSystem::deployFireGuards()
{
	// SetAllGuards(8): guards 0-2 take the office floor, 3-5 the floor
	// below, each repolling a floor-index-mod-6 partition (GuardT).
	guardOffices.clear();
	Game::ItemSetByString::const_iterator it = game->itemsByType.find("security");
	if (it == game->itemsByType.end()) { fireGuardsActive = false; return; }
	for (Game::ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i) {
		GuardOffice office;
		office.floor = (*i)->position.y;
		office.up = office.floor;
		office.down = office.floor - 1;
		for (int gi = 0; gi < kGuardsPerOffice; gi++) {
			Guard & g = office.g[gi];
			g.below = gi >= 3;
			g.floor = g.below ? office.floor - 1 : office.floor;
			g.x = -1; // edge state: pull a burning floor immediately
			g.transit = 0;
			g.pause = 0;
			g.douse = false;
			g.retired = false;
		}
		guardOffices.push_back(office);
	}
	fireGuardsActive = !guardOffices.empty();
}

void EventSystem::destroyFireCell(int floor, int x)
{
	// Destroyed cells become rubble until rebuilt (BurnT records). No
	// cash damage charge (the EXE's fire never touches money) — the loss
	// is the burned buildings themselves.
	addRubble(floor, x);
	Game::ItemSetByInt::const_iterator it = game->itemsByFloor.find(floor);
	if (it == game->itemsByFloor.end()) return;
	for (Game::ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i) {
		Item::Item * item = *i;
		recti r = item->getRect();
		if (x < r.minX() || x >= r.maxX()) continue;
		item->evaluation = std::max(0.0, item->evaluation - 20.0);
		for (Item::Item::People::iterator p = item->people.begin(); p != item->people.end(); ++p)
			(*p)->addStress(10.0);
	}
}

void EventSystem::stepFireFrame()
{
	fireFrame++;
	const int top = topBuiltFloor();

	// Horizontal fronts advance 1 cell / 7 frames; floors above the
	// origin ignite on the origin cell 80 frames per floor later, never
	// below the origin (SpreadFire 10e8:0304).
	for (int f = fireOriginFloor; f <= top; f++) {
		int L, R;
		if (!floorExtent(f, L, R)) continue;

		std::map<int, int>::iterator itL = fireLeft.find(f);
		if (itL == fireLeft.end()) {
			if (f > fireOriginFloor &&
			    fireFrame == (f - fireOriginFloor) * kFireFloorFrames &&
			    fireOriginCell >= L && fireOriginCell < R)
				fireLeft[f] = fireOriginCell;
		} else {
			int l = itL->second;
			destroyFireCell(f, l);
			if (fireFrame % kFireSpreadFrames == 0) l--;
			if (l < L) fireLeft.erase(itL);
			else itL->second = l;
		}

		std::map<int, int>::iterator itR = fireRight.find(f);
		if (itR == fireRight.end()) {
			if (f > fireOriginFloor &&
			    fireFrame == (f - fireOriginFloor) * kFireFloorFrames &&
			    fireOriginCell >= L && fireOriginCell + kFireCells <= R)
				fireRight[f] = fireOriginCell;
		} else {
			int r = itR->second;
			destroyFireCell(f, r + kFireCells); // the cell the front advances into
			if (fireFrame % kFireSpreadFrames == 0) r++;
			if (r + kFireCells > R) fireRight.erase(itR);
			else itR->second = r;
		}
	}

	// Helicopter pass (10e8:0450): 1 cell left per frame, dousing every
	// front at/right of it.
	if (chopperX > 0) {
		chopperX--;
		for (std::map<int, int>::iterator it = fireLeft.begin(); it != fireLeft.end();) {
			if (it->second >= chopperX) it = fireLeft.erase(it); else ++it;
		}
		for (std::map<int, int>::iterator it = fireRight.begin(); it != fireRight.end();) {
			if (it->second >= chopperX) it = fireRight.erase(it); else ++it;
		}
		// DIVERGENCE (ORIGINAL-BUGS B7): the EXE stops at the origin
		// floor's LEFT extent, letting a front that slipped left on a
		// wider floor survive the sweep. We sweep to the ACTUAL leftmost
		// burning front instead, so the blaze always goes out.
		if (!fireBurning()) chopperX = 0;
	}

	if (!fireBurning()) endFire();
}

void EventSystem::stepFireGuards()
{
	// FireGuardTick 10f8:0c06: a 5-frame action pause ends in a douse
	// that kills the whole front the guard stands in; otherwise sweep
	// left 1 cell / 2 frames, pulling a new burning floor at the edge.
	const int top = topBuiltFloor();
	for (size_t oi = 0; oi < guardOffices.size(); ++oi) {
		GuardOffice & office = guardOffices[oi];
		for (int gi = 0; gi < kGuardsPerOffice; gi++) {
			Guard & g = office.g[gi];
			if (g.transit > 0) { g.transit--; continue; }
			if (g.pause > 0) {
				if (--g.pause == 0 && g.douse) {
					int L, R;
					if (floorExtent(g.floor, L, R)) {
						std::map<int, int>::iterator itL = fireLeft.find(g.floor);
						if (itL != fireLeft.end() &&
						    g.x >= itL->second && g.x < itL->second + kFireCells)
							fireLeft.erase(itL);
						else {
							std::map<int, int>::iterator itR = fireRight.find(g.floor);
							if (itR != fireRight.end() &&
							    g.x >= itR->second + kFireCells / 2 &&
							    g.x < itR->second + kFireCells)
								fireRight.erase(itR);
						}
					}
					g.douse = false;
					if (!fireBurning()) { endFire(); return; }
				}
				continue;
			}

			int L, R;
			const bool built = floorExtent(g.floor, L, R);
			const bool atEdge = !built || g.x < 0 || g.x <= L;
			const bool floorClear = built &&
				fireLeft.find(g.floor) == fireLeft.end() &&
				fireRight.find(g.floor) == fireRight.end();
			if (atEdge || floorClear) {
				// Partition by guard index; at the edge the highest
				// burning floor wins, mid-floor the first.
				int pick = -1;
				for (int f = fireOriginFloor + gi; f <= top; f += kGuardsPerOffice) {
					if (fireLeft.count(f) || fireRight.count(f)) {
						pick = f;
						if (!atEdge) break;
					}
				}
				if (pick < 0) continue;
				int Lp, Rp;
				if (!floorExtent(pick, Lp, Rp)) continue;
				int d = pick - g.floor;
				if (d < 0) d = -d;
				g.transit = kGuardFloorFrames * d;
				g.floor = pick;
				g.x = Rp - 2;
				continue;
			}

			g.x--;
			std::map<int, int>::iterator itL = fireLeft.find(g.floor);
			std::map<int, int>::iterator itR = fireRight.find(g.floor);
			const bool lead =
				(itL != fireLeft.end() && g.x >= itL->second && g.x < itL->second + kFireCells / 2) ||
				(itR != fireRight.end() && g.x >= itR->second + kFireCells / 2 &&
				 g.x < itR->second + kFireCells);
			if (lead) { g.pause = kGuardDouseFrames; g.douse = true; }
			else       { g.pause = kGuardSweepFrames - 1; }
		}
	}
}

void EventSystem::endFire()
{
	type = Type::None;
	fireLeft.clear();
	fireRight.clear();
	chopperX = 0;
	fireChopperPending = false;
	fireChopperPaid = false;
	fireGuardsActive = false;
	guardOffices.clear();
	fireFrame = 0;
	fireFrameAccum = 0;

	game->timeWindow.showMessage("The fire was stopped. The tower is damaged.");
	LOG(IMPORTANT, "Fire extinguished");

	// ExtinguishFire (10f0:02ec): skip ahead only when ft is before 16:00.
	if (clockFrame() < kCleanupFrame)
		game->time.advanceToEventFrame(kCleanupFrame);
	prevAbsolute = game->time.absolute;
}

// -------------------------------------------------------------------- bomb

void EventSystem::offerBomb()
{
	if (type != Type::None) return;
	if (!hasSecurity()) return;

	// The ransom switch (10c8:006e) has cases only for stars 2/3/4.
	// game->rating is 0-based, so the star is rating + 1.
	int ransom;
	switch (game->rating + 1) {
		case 2: ransom = kBombRansom[0]; break;
		case 3: ransom = kBombRansom[1]; break;
		case 4: ransom = kBombRansom[2]; break;
		default: return;
	}

	const int floor = pickDisasterFloor();
	if (floor == INT_MIN) return;
	int L, R;
	if (!floorExtent(floor, L, R)) return;
	if (R - L <= 3) return; // extent must exceed 3 cells

	type = Type::Terror;
	bombFloor = floor;
	bombSlot  = L + rand() % (R - 4 - L + 1); // RandomRange(left, right - 4)
	bombRansom = ransom;
	bombArmed = false;
	bombCaught = false;
	guardOffices.clear();

	// The floor is NEVER named (B9 dead code: guards don't know it either).
	char text[192];
	snprintf(text, sizeof(text),
	         "They demand $%d or a hidden bomb will explode at 3 o'clock.", ransom);
	game->askEventChoice("Blackmail from Terrorists!", text, "Pay Them", "Find the Bomb",
		[this](bool pay) { if (type == Type::Terror) resolveBombResponse(pay); });
	game->timeWindow.showMessage("A bomb threat has been received. The bomb's location is unknown!");
	LOG(IMPORTANT, "Bomb threat: ransom $%d (floor %d hidden)", ransom, floor);
}

void EventSystem::resolveBombResponse(bool pay)
{
	if (type != Type::Terror) return;
	if (pay) {
		if (game->funds >= bombRansom) {
			game->transferFunds(-bombRansom, "emergency", "Terrorist ransom paid");
			game->timeWindow.showMessage("The ransom was paid. The terrorists stood down.");
			type = Type::None;
			return;
		}
		game->timeWindow.showMessage("You cannot afford the ransom.");
		// Cannot pay -> the bomb is armed, as if refused.
	}
	// Refusing (or failing to pay) ARMS the bomb (TryStartEvent's refuse branch).
	armBomb();
}

void EventSystem::armBomb()
{
	bombArmed = true;
	deployBombGuards();
	game->timeWindow.showMessage("Security forces are hunting for the bomb. Find it before 1 o'clock!");
}

void EventSystem::deployBombGuards()
{
	guardOffices.clear();
	Game::ItemSetByString::const_iterator it = game->itemsByType.find("security");
	if (it == game->itemsByType.end()) return;
	for (Game::ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i) {
		GuardOffice office;
		office.floor = (*i)->position.y;
		office.up = office.floor;
		office.down = office.floor - 1;
		for (int gi = 0; gi < kGuardsPerOffice; gi++) {
			Guard & g = office.g[gi];
			g.below = gi >= 3;
			g.floor = g.below ? office.floor - 1 : office.floor;
			g.x = -1;
			g.transit = 0;
			g.pause = 0;
			g.douse = false;
			g.retired = false;
		}
		guardOffices.push_back(office);
	}
}

int EventSystem::nextGuardFloor(GuardOffice & office, const Guard & g) const
{
	for (int attempt = 0; attempt < 2; attempt++) {
		const bool down = (attempt == 0) ? g.below : !g.below;
		for (;;) {
			int f = down ? office.down - 1 : office.up + 1;
			if (f < kGuardMinFloor || f > kGuardMaxFloor) break;
			if (down) office.down--; else office.up++;
			if (f >= 2 && f <= lobbyHeight()) continue; // lobby upper stories
			int L, R;
			if (!floorExtent(f, L, R)) continue; // unbuilt
			return f;
		}
	}
	return INT_MIN;
}

void EventSystem::stepBombGuards()
{
	// guard_hunt_step_frame: 1 cell / 2 frames, 3 transit frames/floor,
	// catch on the EXACT cell (CheckEventTarget 10c8:01c4).
	for (size_t oi = 0; oi < guardOffices.size(); ++oi) {
		GuardOffice & office = guardOffices[oi];
		for (int gi = 0; gi < kGuardsPerOffice; gi++) {
			Guard & g = office.g[gi];
			if (g.retired) continue;
			if (g.transit > 0) { g.transit--; continue; }
			if (g.pause > 0)   { g.pause--; continue; }

			int L, R;
			const bool built = floorExtent(g.floor, L, R);
			if (built && g.x >= 0 && g.x > L) {
				g.x--;
				if (g.floor == bombFloor && g.x == bombSlot) {
					bombCaught = true;
					type = Type::None;
					guardOffices.clear();
					game->timeWindow.showMessage("Security found the bomb! Crisis averted.");
					LOG(IMPORTANT, "Security caught the bomb on floor %d", bombFloor);
					game->time.advanceToEventFrame(kCleanupFrame);
					prevAbsolute = game->time.absolute;
					return;
				}
				g.pause = kGuardSweepFrames - 1;
			} else {
				const int f = nextGuardFloor(office, g);
				if (f == INT_MIN) { g.retired = true; continue; }
				int d = f - g.floor;
				if (d < 0) d = -d;
				g.transit = kGuardFloorFrames * d;
				g.floor = f;
				int Lf, Rf;
				g.x = floorExtent(f, Lf, Rf) ? Rf - 2 : -1;
			}
		}
	}
}

void EventSystem::detonateBomb()
{
	LOG(IMPORTANT, "Bomb detonated on floor %d cell %d", bombFloor, bombSlot);

	// DestroyTenants (10c8:02bd): everything touching floors
	// [t-2 .. t+3] x cells [t-20 .. t+20] becomes rubble. No cash charge.
	const int minF = bombFloor - kBombBlastFloorsDown;
	const int maxF = bombFloor + kBombBlastFloorsUp;
	const int minX = bombSlot - kBombBlastHalfCells;
	const int maxX = bombSlot + kBombBlastHalfCells;

	for (int f = minF; f <= maxF; f++)
		for (int x = minX; x <= maxX; x++)
			addRubble(f, x);

	for (Game::ItemSet::iterator i = game->items.begin(); i != game->items.end(); ++i) {
		Item::Item * item = *i;
		recti r = item->getRect();
		if (r.maxY() <= minF || r.minY() > maxF) continue;
		if (r.maxX() <= minX || r.minX() > maxX) continue;
		item->evaluation = std::max(0.0, item->evaluation - 60.0);
		for (Item::Item::People::iterator p = item->people.begin(); p != item->people.end(); ++p)
			(*p)->addStress(60.0);
	}

	game->timeWindow.showMessage("BOMB DETONATED! Extensive damage reported.");
	type = Type::None;
	bombArmed = false;
	guardOffices.clear();
	game->time.advanceToEventFrame(kCleanupFrame);
	prevAbsolute = game->time.absolute;
}

// ---------------------------------------------------------------- treasure

bool EventSystem::treasureCondition(int star) const
{
	// 1148:01ac: at stars 1/2/3 the basement floor B(star+2) must be
	// built with an extent wider than star * 25 cells. Star > 3 always
	// fails (parks the latch harmlessly armed).
	if (star < 1 || star > 3) return false;
	int L, R;
	if (!floorExtent(-(star + 2), L, R)) return false;
	return (R - L) > star * kTreasureExtentPerStar;
}

void EventSystem::payTreasure(int star)
{
	const int award = kTreasureAwards[star - 1];
	treasureLatched = true;
	// AwardMoney 1178:076f clamps the cash field at $99,999,999.
	game->transferFunds(award, "treasure", "Buried treasure discovered");
	if (game->funds > kCashCap) game->setFunds(kCashCap);
	char buf[192];
	snprintf(buf, sizeof(buf),
	         "During construction, workers discovered ancient buried treasure! It sold for $%d.", award);
	game->timeWindow.showMessage(buf);
	LOG(IMPORTANT, "Buried treasure worth $%d (star %d dig)", award, star);
}

void EventSystem::armTreasureLatch(int star)
{
	// LevelT @0047-004a: re-seed with the NEW tier's dig condition.
	treasureLatched = treasureCondition(star);
}

void EventSystem::notifyBuilt()
{
	// NotifyFacilityBuilt (1148:0163): on every successful build an armed
	// latch plus the dig condition pays the star-tier award once.
	for (Game::ItemSet::iterator i = game->items.begin(); i != game->items.end(); ++i) {
		Item::Item * item = *i;
		if (item->underConstruction) {
			pendingBuilds.insert(item);
		} else if (pendingBuilds.erase(item) != 0) {
			const int star = game->rating + 1; // rating is 0-based
			if (!treasureLatched && treasureCondition(star))
				payTreasure(star);
		}
	}
}

// --------------------------------------------------------- debug triggers

// Hidden debug menu (EXE actions 9002/9000/9001). These fire the same
// entry points the calendar dispatch uses; their own gating (no active
// event, security office, star ladder) still decides — only the calendar
// scheduling is bypassed.

void EventSystem::debugTriggerFire()
{
	startFire();
}

void EventSystem::debugTriggerTerror()
{
	offerBomb();
}

void EventSystem::debugTriggerTreasure()
{
	// The dig ladder tops out at star 3 (tuning +0xA8..0xAC); clamp so the
	// payout index stays in range at 4/5 stars.
	const int star = std::max(1, std::min(3, game->rating + 1));
	payTreasure(star);
}

// ---------------------------------------------------------------- helpers

double EventSystem::elapsedFrames() const
{
	// Absolute time is cumulative, so this also handles midnight rollover.
	// Capped at one day so a hitch or loaded save cannot flood the steppers.
	return std::min(static_cast<double>(Time::kSimFramesPerDay),
	                std::max(0.0, game->time.eventFramesSince(prevAbsolute)));
}

int EventSystem::clockFrame() const
{
	return game->time.eventFrame();
}

// ---------------------------------------------------------------- advance

void EventSystem::advance(double dt)
{
	(void)dt;

	// --- Calendar (TimeT 1208:0342) ---
	if (game->time.checkHour(kDispatchHour))
		dailyDispatch();
	if (game->time.checkHour(21.0) &&
	    (int)std::floor(game->time.absolute) % kSantaPeriodDays == kSantaPeriodDays - 1 &&
	    !santaActive)
		launchSanta();

	const double frames = elapsedFrames();
	prevAbsolute = game->time.absolute;

	// --- Santa fly-by (SantaT seg_11b8) ---
	if (santaActive) {
		santaX -= (float)(kSantaTickPixels * frames);
		int phase = ((int)santaX >> 3) & 15; // 0..15
		santaY = kSantaBaseY + (phase < 8 ? phase : 16 - phase);
		if (santaX < -200.f) santaActive = false;
	}

	// --- Buried treasure: watch for completed builds ---
	notifyBuilt();

	// --- Bomb deadline (checked before the guard sweep: a guard can't
	// catch it on the deadline frame) ---
	if (type == Type::Terror && clockFrame() >= kBombDetonateFrame) {
		detonateBomb();
		return;
	}

	// --- Per-EXE-frame event stepping ---
	fireFrameAccum += frames;
	int iterations = 0;
	while (fireFrameAccum >= 1.0 && type != Type::None && iterations < 4000) {
		fireFrameAccum -= 1.0;
		iterations++;
		if (type == Type::Fire) {
			if (clockFrame() >= kFireEndFrame) { endFire(); break; }
			stepFireFrame();
			if (type == Type::Fire && fireGuardsActive) stepFireGuards();
		} else if (type == Type::Terror) {
			stepBombGuards();
		}
	}
	if (type == Type::None) fireFrameAccum = 0;
	// Re-sync after any clock jump the event performed (cleanup 4 PM).
	prevAbsolute = game->time.absolute;
}

bool EventSystem::handleClick(int2 tile)
{
	if (type == Type::Fire && !fireChopperPending && !fireChopperPaid && chopperX == 0) {
		// Accept a click within the burning area (the hidden mid-fire
		// chopper purchase, 10e8:01e2).
		for (std::map<int, int>::const_iterator it = fireLeft.begin(); it != fireLeft.end(); ++it)
			if (abs(tile.x - it->second) <= kFireCells && abs(tile.y - it->first) <= 2) {
				callChopper();
				return true;
			}
		for (std::map<int, int>::const_iterator it = fireRight.begin(); it != fireRight.end(); ++it)
			if (abs(tile.x - it->second) <= kFireCells && abs(tile.y - it->first) <= 2) {
				callChopper();
				return true;
			}
	}
	return false;
}

// ------------------------------------------------------------------ render

void EventSystem::draw(sf::RenderTarget & target, sf::RenderStates /*states*/) const
{
	// Rubble from past fires and blasts persists until rebuilt.
	for (std::set<long>::const_iterator it = rubble.begin(); it != rubble.end(); ++it) {
		int floor, x;
		decodeTile(*it, floor, x);
		Sprite s = rubbleSprite;
		s.setPosition(tileToPixel(x, floor));
		target.draw(s);
		game->drawnSprites++;
	}

	if (type == Type::Fire) {
		int frame = (fireFrame / 3) % 4;
		sf::IntRect fireRect({frame * 96, 0}, {96, 36});

		// A freshly ignited floor (its two fronts still within one 12-cell
		// strip of each other) burns with small flames; once the fronts
		// have separated the floor is a full blaze (large flames).
		for (int side = 0; side < 2; side++) {
			const std::map<int, int> & fronts = side ? fireRight : fireLeft;
			for (std::map<int, int>::const_iterator it = fronts.begin(); it != fronts.end(); ++it) {
				const bool small = fireFloorIgniting(it->first);
				Sprite f = small ? smallFireSprite : fireSprite;
				if (!small) f.setTextureRect(fireRect);
				f.setPosition(tileToPixel(it->second, it->first));
				target.draw(f);
				game->drawnSprites++;
			}
		}

		if (chopperX > 0) {
			// Rotor animation: same 4-frame / 3-frame cadence as the flames.
			Sprite chop = chopperSprite;
			chop.setTextureRect(fireRect);
			chop.setPosition(tileToPixel(chopperX, fireOriginFloor));
			target.draw(chop);
			game->drawnSprites++;
		} else if (!fireChopperPaid) {
			Sprite a = alertSprite;
			a.SetImage(App->bitmaps["simtower/alerts/fire"]);
			a.setOrigin({38.f, 60.f});
			a.setPosition({tileToPixel(fireOriginCell, fireOriginFloor).x,
			               tileToPixel(fireOriginCell, fireOriginFloor).y - 40.f});
			target.draw(a);
			game->drawnSprites++;
		}
	}
	// The bomb is never marked on screen — its floor is hidden from the
	// player (guards hunt it blind).

	if (santaActive) {
		Sprite s = santaSprite;
		s.setPosition({santaX, santaY});
		target.draw(s);
		game->drawnSprites++;
	}
}

// -------------------------------------------------------------- persistence

void EventSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	xml.PushAttribute("eventTreasureLatched", treasureLatched);
	xml.PushAttribute("eventSantaActive", santaActive);
	if (santaActive) {
		xml.PushAttribute("eventSantaX", santaX);
		xml.PushAttribute("eventSantaY", santaY);
	}
	std::string r;
	for (std::set<long>::const_iterator it = rubble.begin(); it != rubble.end(); ++it) {
		char buf[32];
		snprintf(buf, sizeof(buf), "%ld ", *it);
		r += buf;
	}
	xml.PushAttribute("eventRubble", r.c_str());
	// Active events are not persisted mid-resolution; they end on reload.
}

void EventSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	treasureLatched = xml.BoolAttribute("eventTreasureLatched", false);
	santaActive = xml.BoolAttribute("eventSantaActive", false);
	santaX = (float)xml.DoubleAttribute("eventSantaX", 0.0);
	santaY = (float)xml.DoubleAttribute("eventSantaY", 0.0);

	rubble.clear();
	std::string r = xml.Attribute("eventRubble") ? xml.Attribute("eventRubble") : "";
	size_t pos = 0;
	while (pos < r.size()) {
		size_t sp = r.find(' ', pos);
		if (sp == std::string::npos) break;
		rubble.insert(atol(r.substr(pos, sp - pos).c_str()));
		pos = sp + 1;
	}

	// A save with an active event reloads safe.
	type = Type::None;
	fireLeft.clear();
	fireRight.clear();
	guardOffices.clear();
	bombArmed = false;
	bombCaught = false;
	pendingBuilds.clear();
	// Old saves contain positions/latches only, no elapsed-frame progress
	// to convert. Active fire/bomb and cathedral ceremonies remain transient.
	fireFrame = 0;
	fireFrameAccum = 0;
	prevAbsolute = game->time.absolute;
}
