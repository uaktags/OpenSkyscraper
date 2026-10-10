/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower half of EventSystem: the T2EV scheduler (T2TowerEvent::Idle
 * 0x10069970) and the events it starts. Fire, terrorist and Santa reuse the
 * SimTower paths in EventSystem.cpp with the FBEV/TREV/SAEV parameters; the
 * blackout is PowerSystem; Uncle Reggae (T2Maru_Reggae 0x1003a770..) and
 * the rooftop decorations (T2OutDecoration 0x10045760..) are native here;
 * the kite, dolphins, Mr. Maru and the hot spring are YootExtrasSystem.
 * See doc/yoottower/game-parity.md V1-V7. */
#include "EventSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "Logger.h"
#include "Person.h"
#include "YootCatalog.h"
#include "YootWorld.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace OT;

namespace
{
	/// Minute of day -> EventSystem's clock frame (hour h = frame 100h).
	int minuteToFrame(int minute) { return minute * 100 / 60; }

	const int kReggaeTickMinutes = 3;    ///< ReggaeReception: +0x50 += 3
	const int kReggaeClaimTicks = 60;    ///< +0x54 % 0x3c: claim + guard call
	const int kReggaeStressTicks = 10;   ///< +0x54 % 10: IncStress(10) per occupant
	const int kReggaeStress = 10;
	const int kGuardMinutesPerFloor = 3; ///< approximation of the guard's walk

	bool isRestaurant(const Item::Item * item)
	{
		// ToolDef category 50 = "Restaurants" (FindTargetTenant compares
		// +0x38 with the tenant ToolDef's +100): the plugin restaurants too.
		if (item->prototype->yoot) return item->prototype->yoot->category == 50;
		const std::string & id = item->prototype->id;
		return id == "fastfood" || id == "restaurant";
	}
}

void EventSystem::syncRuleset()
{
	const RulesProfile & profile = game->activeRules.profile();
	const bool wantYoot = game->activeRules.ruleset() == BuildRules::Ruleset::YootTower &&
	                      !profile.events.empty();
	char key[96] = "";
	if (wantYoot)
		snprintf(key, sizeof(key), "%s:%d:%d", profile.worldId.c_str(),
		         (int)profile.events.size(), profile.worldWidth);
	if (wantYoot == yoot && yootKey == key) return;

	yoot = wantYoot;
	yootKey = key;
	chopperCost = 500000;
	fireEndFrame = 2000;
	bombDetonateFrame = 1300;
	yootFireIdx = yootTerrorIdx = yootBlackoutIdx = yootSantaIdx = -1;
	reggae = Reggae();
	if (!yoot) {
		scheduler = YootEventScheduler();
		power.configure(nullptr);
		return;
	}

	scheduler.setEvents(profile.events, profile.worldWidth);
	yootFireIdx = scheduler.find("FBEV");
	yootTerrorIdx = scheduler.find("TREV");
	yootBlackoutIdx = scheduler.find("BOEV");
	yootSantaIdx = scheduler.find("SAEV");
	const std::vector<YootEventSpec> & ev = scheduler.events();

	YootFireDef fire;
	if (yootFireIdx >= 0 && decodeYootFire(ev[yootFireIdx], fire)) {
		chopperCost = fire.heliPrice * 100;            // $300,000
		fireEndFrame = minuteToFrame(ev[yootFireIdx].endMin); // Exec stops at endTime (18:00)
	}
	YootTerroristDef terror;
	if (yootTerrorIdx >= 0 && decodeYootTerrorist(ev[yootTerrorIdx], terror))
		bombDetonateFrame = minuteToFrame(terror.deadlineMin); // 12:00
	power.configure(yootBlackoutIdx >= 0 ? &ev[yootBlackoutIdx] : nullptr);
	LOG(INFO, "YootTower events: %d from %s (fire $%d, bomb at frame %d, power %s)",
	    (int)ev.size(), profile.worldId.c_str(), chopperCost, bombDetonateFrame,
	    power.enabled() ? "on" : "off");
}

void EventSystem::yootSyncRunning()
{
	scheduler.setRunning(yootFireIdx, type == Type::Fire);
	scheduler.setRunning(yootTerrorIdx, type == Type::Terror);
	scheduler.setRunning(yootBlackoutIdx, power.isBlackOut());
	scheduler.setRunning(yootSantaIdx, santaActive);
	if (reggae.index >= 0) scheduler.setRunning(reggae.index, reggae.active);
	extras.syncRunning(scheduler);
}

void EventSystem::yootDispatch()
{
	const int day = (int)std::floor(game->time.absolute);
	int minute = (int)std::floor(game->time.hour * 60.0);
	minute = std::max(0, std::min(1439, minute));
	const int grade = game->rating + 1;

	yootSyncRunning();
	const std::vector<int> begins = scheduler.advance(day, minute, grade);
	for (size_t i = 0; i < begins.size(); ++i)
		yootBegin(begins[i]);

	power.advance();
	facilities.advance();
	venues.advance();
	specials.advance();
	extras.advance();
	people.advance();
	if (reggae.active) stepReggae(minute);
	yootSyncRunning();
}

void EventSystem::yootBegin(int index)
{
	const YootEventSpec & e = scheduler.events()[index];
	if (extras.begin(e, index)) return; // DOEV, TOEV, MAEV 128 (Mr. Maru)
	if (e.type == "FBEV") {
		yootFireStart();
	} else if (e.type == "TREV") {
		offerBomb();
	} else if (e.type == "BOEV") {
		power.startCheck();
	} else if (e.type == "SAEV") {
		// T2SantaClaus::Start 0x10056200: x = width - lapse, at topFloorLine.
		santaActive = true;
		santaMinutes = 0;
		santaStartMinute = yootEventBeginMinute(e, game->activeRules.profile().worldWidth);
		LOG(IMPORTANT, "Santa flies across the world");
	} else if (e.type == "MAEV") {
		YootReggaeDef def;
		if (decodeYootReggae(e, def) && def.isReggae())
			startReggae(index); // Mr. Maru is YootExtrasSystem's
	} else if (e.type == "ODEV") {
		// Persistent: drawn every frame (drawOutDecorations).
	} else if (e.type == "MREV") {
		// The timetable itself runs in VenueSystem::subwayTick.
		LOG(DEBUG, "MREV begins: subway timetable");
	} else if (e.type == "BBEV") {
		venues.beginBaseball(); // T2Baseball (Tokyo.t2p 0x100118d0..)
	} else if (e.type == "WDEV") {
		venues.beginWedding();  // T2Wedding (Hawai.t2p 0x10014ed0..)
	} else if (e.type == "CLEV") {
		// Persistent: VenueSystem moves and draws the clouds.
	} else if (e.type == "HSEV") {
		// T2HotSpring watches the builds: YootExtrasSystem::onBuilt.
	} else {
		LOG(DEBUG, "%s begins: world X-event hook", e.type.c_str());
	}
}

// -------------------------------------------------------------------- fire
// T2FireBurning (Tokyo.t2p 0x10030d50..0x10032590), see game-parity.md 7.6.K.

namespace
{
	const char * ordinal(int n)
	{
		const int t = n % 100;
		if (t >= 11 && t <= 13) return "th";
		switch (n % 10) { case 1: return "st"; case 2: return "nd"; case 3: return "rd"; default: return "th"; }
	}

	/// The tower the fire reads: any tenant (not a floor, the lobby or a mover) covers a cell.
	class TowerFireGrid : public Yoot::FireGrid
	{
	public:
		TowerFireGrid(EventSystem & e) : es(e) {}
		Yoot::FireCell cell(int floor, int x) const override { return es.yootFireCell(floor, x); }
		void burn(int floor, int x, int & lo, int & hi) override { es.yootFireBurn(floor, x, lo, hi); }
	private:
		EventSystem & es;
	};

	Item::Item * tenantAt(Game * game, int floor, int x)
	{
		Game::ItemSetByInt::const_iterator it = game->itemsByFloor.find(floor);
		if (it == game->itemsByFloor.end()) return nullptr;
		for (Item::Item * item : it->second) {
			if (item->isElevator() || item->isStairlike() || item->isLobby() || item->canHaulPeople()) continue;
			if (item->prototype->id == "floor") continue;
			const recti r = item->getRect();
			if (x >= r.minX() && x < r.maxX() && floor >= r.minY() && floor < r.maxY()) return item;
		}
		return nullptr;
	}
}

Yoot::FireCell EventSystem::yootFireCell(int floor, int x) const
{
	if (hasRubble(floor, x)) return Yoot::FireCell::Burnt;
	Item::Item * item = tenantAt(game, floor, x);
	if (!item) return Yoot::FireCell::None;
	return item->survivesDisaster() ? Yoot::FireCell::Fireproof : Yoot::FireCell::Tenant;
}

void EventSystem::yootFireBurn(int floor, int x, int & lo, int & hi)
{
	Item::Item * item = tenantAt(game, floor, x);
	if (!item) { lo = x; hi = x + 1; addRubble(floor, x); return; }
	const recti r = item->getRect();
	lo = r.minX();
	hi = r.maxX();
	game->destroyItemByDisaster(item); // T2Tenant::BurntDown: the tenant stays as rubble
}

void EventSystem::yootFireStart()
{
	if (type != Type::None) return;
	// T2TowerEvent::Idle calls Start once, at the begin minute; an event whose Start
	// fails is dropped for the day. IsBreakoutFire gets 100 random cells of the world.
	const RulesProfile & profile = game->activeRules.profile();
	const int width = profile.worldWidth > 0 ? profile.worldWidth : 368;
	const int rows = profile.buildBottomRow != INT_MAX ? profile.buildBottomRow : profile.groundRow + 11;
	TowerFireGrid grid(*this);
	int floor = 0, x = 0;
	// The random column is a world unit (0 .. width); items are in OpenSky x.
	const bool found = yootFire.breakout(grid, width, rows, [] { return rand(); },
		[&](int row) { return profile.groundRow - 1 - row; }, profile.unitOriginX, floor, x);
	if (!found) {
		LOG(IMPORTANT, "Yoot fire: no breakout cell, no fire today");
		return;
	}
	yootFire = Yoot::FireModel();
	yootFire.travelMoves = [](const Yoot::Blaze &) { return 40; };
	if (!yootFire.makeFire(grid, x, floor)) return;

	type = Type::Fire;
	yootFireMode = 0;
	yootFireStartFloor = floor;
	yootHeliManual = false;
	yootFireIdles = (long long)std::floor(game->time.absolute * 1440.0 * 4.0);
	fireFrameAccum = 0;
	game->evacuateForDisaster();

	char buf[320];
	char label[16];
	if (floor < 0) snprintf(label, sizeof(label), "B%d", -floor);
	else snprintf(label, sizeof(label), "%d%s", floor + 1, ordinal(floor + 1));
	snprintf(buf, sizeof(buf), "Fire has broken out on the %s floor. The residents are escaping through the "
	         "emergency stairs. Please put the fire out immediately!", label);
	LOG(IMPORTANT, "Yoot fire on floor %d at x %d", floor, x);
	// Two dialogs in the original: the notice, then T2DLG 8110 with the three responses.
	game->askEventChoice("Event", buf, "OK", "", [this](bool) {
		if (type != Type::Fire) return;
		const std::vector<std::string> labels = {"Firefighting team", "The security guard", "Leave"};
		char text[160];
		snprintf(text, sizeof(text), "Please select a way to put the fire out.\n(helicopter: $%d)", chopperCost);
		game->askEventChoices("Event", text, labels, [this](int i) { return yootFireChoose(i); });
	});
}

bool EventSystem::yootFireChoose(int index)
{
	if (type != Type::Fire || yootFireMode != 0) return true;
	// GetTenantByPID('IBEK'): the Security Room itself, not the Fire Department.
	Game::ItemSetByString::const_iterator sec = game->itemsByType.find("security");
	const bool securityRoom = sec != game->itemsByType.end() && !sec->second.empty();
	const Yoot::FireResponse r = Yoot::fireResponse(index, game->funds, chopperCost, securityRoom);
	if (!r.accepted) {
		game->timeWindow.showMessage(r.message);
		return false;
	}
	yootFireMode = r.mode;
	if (r.mode == 1) {
		game->transferFunds(-(int)r.cost, "emergency", "Fire helicopter dispatched");
		const Yoot::Blaze & b = yootFire.fires.front();
		yootHeliX = (float)(b.left + 80);
		yootHeliY = -b.floor * 36.0f - 5 * 36.0f;
		game->timeWindow.showMessage(r.message);
		LOG(IMPORTANT, "Yoot fire: helicopter ($%d)", (int)r.cost);
	} else if (r.mode == 2) {
		// CallFireman 0x10031ed0: six people in all from the registry-5 tenants.
		int sent = 0;
		const std::vector<std::pair<Item::Item *, int> > crew = specials.firemen();
		for (size_t i = 0; i < crew.size(); ++i) sent += crew[i].second;
		yootFire.firemen = std::min(6, sent);
		LOG(IMPORTANT, "Yoot fire: %d firemen", yootFire.firemen);
	}
	yootFireIdles = (long long)std::floor(game->time.absolute * 1440.0 * 4.0);
	return true;
}

void EventSystem::yootHeliMove()
{
	// T2Helicopter::Move: it flies towards the cursor, easing in; here towards the last click,
	// else the nearest fire, and a fire under the water goes out (DoExtinguish).
	const std::vector<Yoot::Blaze> & fires = yootFire.fires;
	if (fires.empty()) return;
	float tx, ty;
	int floor;
	if (yootHeliManual) {
		tx = (float)yootHeliTargetX; floor = yootHeliTargetFloor;
	} else {
		size_t best = 0;
		float bestD = 1e30f;
		for (size_t i = 0; i < fires.size(); ++i) {
			const float d = std::fabs((fires[i].left + 48) - yootHeliX) + 200.0f * std::abs((int)(-fires[i].floor * 36.0f - yootHeliY) / 36);
			if (d < bestD) { bestD = d; best = i; }
		}
		tx = (float)(fires[best].left + 48); floor = fires[best].floor;
	}
	ty = -floor * 36.0f;
	const float dx = tx - yootHeliX, dy = ty - yootHeliY;
	const float dist = std::sqrt(dx * dx + dy * dy);
	const float speed = 24.0f; // px per idle
	if (dist > speed) { yootHeliX += dx / dist * speed; yootHeliY += dy / dist * speed; }
	else { yootHeliX = tx; yootHeliY = ty; }
	if (dist <= 8.0f || dist <= speed) {
		if (yootFire.extinguishAt((int)tx, floor)) yootHeliManual = false;
		else if (yootHeliManual) yootHeliManual = false;
	}
}

void EventSystem::yootFireStep()
{
	if (yootFireMode == 0) return; // the dialogs hold the fire
	const long long now = (long long)std::floor(game->time.absolute * 1440.0 * 4.0 + 1e-9);
	long long steps = now - yootFireIdles;
	if (steps <= 0) return;
	if (steps > 4000) steps = 4000;
	yootFireIdles = now;
	TowerFireGrid grid(*this);
	const int endMinute = yootFireIdx >= 0 ? scheduler.events()[yootFireIdx].endMin : 1080;
	for (long long i = 0; i < steps && type == Type::Fire; ++i) {
		if (yootFireMode == 1) yootHeliMove();
		yootFire.idle(grid, yootFireMode == 2);
		// Exec: carry on while fires remain and the clock is not at the end time.
		const int minute = (int)std::floor(game->time.hour * 60.0);
		if (yootFire.fires.empty() || minute >= endMinute) { yootFireEnd("Fire has been put out."); return; }
	}
}

void EventSystem::yootFireEnd(const char * message)
{
	// StopEvent 0x10031810: every fire goes out, the game goes back to its speed.
	yootFire.fires.clear();
	yootFireMode = 0;
	yootHeliManual = false;
	type = Type::None;
	game->timeWindow.showMessage(message);
	LOG(IMPORTANT, "Yoot fire over: %s", message);
	prevAbsolute = game->time.absolute;
}

void EventSystem::yootAdvanceSanta(double frames)
{
	if (!santaActive) return;
	const RulesProfile & profile = game->activeRules.profile();
	const int width = profile.worldWidth > 0 ? profile.worldWidth : 368;
	santaMinutes += frames * 0.6; // 100 frames per hour
	// T2SantaClaus::Exec 0x10056440: one unit left per elapsed minute,
	// done once the lapse exceeds the world width.
	const double unit = width - santaMinutes;
	santaX = (float)((unit - width / 2) * 8.0);
	const int top = profile.highestFloor == INT_MAX ? 60 : profile.highestFloor;
	int phase = ((int)santaX >> 3) & 15;
	santaY = -(top + 1) * 36.0f + (phase < 8 ? phase : 16 - phase);
	if (santaMinutes > width) santaActive = false;
}

// ------------------------------------------------------------ Uncle Reggae

void EventSystem::startReggae(int index)
{
	YootReggaeDef def;
	if (!decodeYootReggae(scheduler.events()[index], def)) return;

	// FindTargetTenant 0x1003b660: a random lobby-reachable tenant of the
	// category, not fireproof. ReggaeReception plays only where people are.
	std::vector<Item::Item *> busy, any;
	for (Game::ItemSet::iterator i = game->items.begin(); i != game->items.end(); ++i) {
		Item::Item * item = *i;
		if (item->underConstruction || item->lobbyRoute.empty()) continue;
		if (def.tenantCategory != 0 && !isRestaurant(item)) continue;
		if (def.tenantCategory == 0 && (item->isElevator() || item->isStairlike() || item->isLobby())) continue;
		any.push_back(item);
		if (!item->people.empty()) busy.push_back(item);
	}
	std::vector<Item::Item *> & pool = busy.empty() ? any : busy;
	if (pool.empty()) return;

	reggae = Reggae();
	reggae.active = true;
	reggae.index = index;
	reggae.def = def;
	reggae.target = pool[rand() % pool.size()];
	reggae.nextMinute = (int)std::floor(game->time.hour * 60.0);
	// With the visitor pool he walks there (VIPL 224: silhouette 355).
	reggae.walker = people.spawnVip(2, 355, reggae.target);
	LOG(IMPORTANT, "Uncle Reggae visits %s on floor %d",
	    reggae.target->prototype->name.c_str(), reggae.target->position.y);
}

void EventSystem::stepReggae(int minute)
{
	if (game->items.find(reggae.target) == game->items.end()) { endReggae(false); return; }
	const YootEventSpec & e = scheduler.events()[reggae.index];
	if (minute >= e.endMin || minute + 720 < reggae.nextMinute) { endReggae(false); return; }
	if (reggae.guardMinute >= 0 && minute >= reggae.guardMinute) { endReggae(true); return; }
	if (reggae.walker)
	{
		if (!people.alive(reggae.walker)) { reggae.walker = nullptr; endReggae(false); return; }
		// ReggaeReception plays at the tenant only.
		if (!reggae.walker->posing) { reggae.nextMinute = minute; return; }
	}

	while (reggae.nextMinute <= minute) {
		reggae.nextMinute += kReggaeTickMinutes;
		reggae.counter++;
		// Walk style 0 <-> 1 every tick: the two guitar frames.
		if (reggae.walker) reggae.walker->walkStyle = reggae.counter & 1;
		if (reggae.counter % kReggaeClaimTicks == 0) {
			// MakeClaimMessage 0x1003b520 ("%s. There was a complaint from
			// %s on the %sth floor RM# %s.") and the security call
			// (FUN_1003b980: the nearest Security Room sends a guard).
			char buf[256];
			const int f = reggae.target->position.y;
			snprintf(buf, sizeof(buf), "%s. There was a complaint from %s on the %d%s floor.",
			         reggae.def.claim.c_str(), reggae.target->prototype->name.c_str(),
			         f >= 0 ? f + 1 : -f, f >= 0 ? "th" : "th basement");
			game->timeWindow.showMessage(buf);
			if (reggae.guardMinute < 0) {
				Game::ItemSetByString::const_iterator sec = game->itemsByType.find("security");
				if (sec != game->itemsByType.end() && !sec->second.empty()) {
					int best = INT_MAX;
					for (Game::ItemSet::const_iterator s = sec->second.begin(); s != sec->second.end(); ++s)
						best = std::min(best, std::abs((*s)->position.y - f));
					reggae.guardMinute = minute + kGuardMinutesPerFloor * std::max(1, best);
				}
			}
		}
		if (reggae.counter % kReggaeStressTicks == 0) {
			for (Item::Item::People::iterator p = reggae.target->people.begin();
			     p != reggae.target->people.end(); ++p)
				(*p)->addStress(kReggaeStress);
		}
	}
}

void EventSystem::endReggae(bool caught)
{
	if (caught) {
		// ReggaeReception state 7 (string 0x100c7988).
		game->timeWindow.showMessage("Uncle Reggae was caught by the security staff.");
		LOG(IMPORTANT, "Uncle Reggae caught by security");
	} else {
		LOG(IMPORTANT, "Uncle Reggae leaves");
	}
	if (reggae.walker) people.vipLeave(reggae.walker, reggae.target);
	reggae.walker = nullptr;
	reggae.active = false;
	reggae.target = nullptr;
}

// ----------------------------------------------------- rooftop decorations

sf::Texture * EventSystem::outDecorationTexture(int imageId) const
{
	std::map<int, sf::Texture>::iterator it = outDecoTextures.find(imageId);
	if (it == outDecoTextures.end()) {
		sf::Texture & tex = outDecoTextures[imageId];
		YootWorld::Id id;
		if (parseYootWorldIdentifier(game->activeRules.profile().worldId, id))
			app->bitmaps.loadFromPlugin(yootWorldPluginFile(id), imageId, tex);
		it = outDecoTextures.find(imageId);
	}
	return it->second.getSize().x ? &it->second : nullptr;
}

void EventSystem::drawOutDecorations(sf::RenderTarget & target) const
{
	const int idx = scheduler.find("ODEV");
	if (idx < 0) return;
	YootOutDecorationDef def;
	if (!decodeYootOutDecoration(scheduler.events()[idx], def) || def.items.empty()) return;

	// T2OutDecoration::MakeLoofList 0x10045c60: roof segments of the
	// buildings standing on 1F (movers are not tenants).
	std::map<int, std::vector<std::pair<int, int> > > spans; // floor -> [x0, x1)
	int minX = INT_MAX, maxX = INT_MIN;
	for (Game::ItemSet::const_iterator i = game->items.begin(); i != game->items.end(); ++i) {
		const Item::Item * item = *i;
		if (item->isElevator() || item->isStairlike()) continue;
		for (int f = item->position.y; f < item->position.y + item->size.y; ++f)
			spans[f].push_back(std::make_pair(item->position.x, item->position.x + item->size.x));
		if (item->position.y <= 0 && item->position.y + item->size.y > 0) {
			minX = std::min(minX, item->position.x);
			maxX = std::max(maxX, item->position.x + item->size.x);
		}
	}
	if (minX >= maxX) return;
	auto occupied = [&](int x, int f) {
		std::map<int, std::vector<std::pair<int, int> > >::const_iterator s = spans.find(f);
		if (s == spans.end()) return false;
		for (size_t k = 0; k < s->second.size(); ++k)
			if (x >= s->second[k].first && x < s->second[k].second) return true;
		return false;
	};

	// DoDecoration 0x10045960: hand out the ornaments in list order, one
	// per segment wider than the sprite + 64 px, at +42 px, bottom 8 px
	// above the roof line. Pattern 1 (lower half) at night.
	const double hour = game->time.hour;
	const int pattern = (hour < 6.0 || hour >= 18.0) ? 1 : 0;
	size_t next = 0;
	int x = minX;
	while (x < maxX && next < def.items.size()) {
		if (!occupied(x, 0)) { ++x; continue; }
		int h = 0;
		while (occupied(x, h)) ++h;
		int end = x + 1;
		while (end < maxX && occupied(end, h - 1) && !occupied(end, h)) ++end;
		const int segPx = (end - x) * 8;
		for (; next < def.items.size(); ++next) {
			sf::Texture * tex = outDecorationTexture(def.items[next].first);
			if (!tex) continue;
			const int patterns = def.items[next].second ? (int)def.items[next].second : 1;
			const int w = (int)tex->getSize().x;
			const int ph = (int)tex->getSize().y / patterns;
			if (w + 64 >= segPx) break; // too narrow: try the next roof
			Sprite s;
			s.SetImage(*tex);
			s.setTextureRect(sf::IntRect({0, std::min(pattern, patterns - 1) * ph}, {w, ph}));
			s.setPosition({(float)(x * 8 + 42), (float)(-h * 36 - 8 - ph)});
			target.draw(s);
			game->drawnSprites++;
			++next;
			break;
		}
		x = end;
	}
}
