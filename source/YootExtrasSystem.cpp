/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower school, cosmetic flights, Mr. Maru, the Kegon hot spring and
 * the traffic map. game-parity.md S2, V4, V6, J2. */
#include "YootExtrasSystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "Item/YootMiscItems.h"
#include "Logger.h"
#include "PeopleSystem.h"
#include "PluginManager.h"
#include "SimHarness.h"
#include "YootEvents.h"
#include "YootRules.h"
#include "YootWorld.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <tinyxml2.h>

using namespace OT;

namespace
{
	const Game::ItemSet & itemsOf(Game * game, const char * id)
	{
		static const Game::ItemSet none;
		Game::ItemSetByString::const_iterator it = game->itemsByType.find(id);
		return it == game->itemsByType.end() ? none : it->second;
	}

	int nowMinuteOfDay(Game * game)
	{
		return std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
	}
}

YootExtrasSystem::YootExtrasSystem(Game * game) : GameObject(game) {}

void YootExtrasSystem::reset()
{
	lastMinute = lastHour = -1;
	trafficKey.clear();
	traffic = Yoot::TrafficMap();
	flights.clear();
	maru = Maru();
	maruSound.reset();
}

bool YootExtrasSystem::enabled() const
{
	return game && game->activeRules.profile().yootWorld;
}

void YootExtrasSystem::configureTraffic()
{
	const RulesProfile & p = game->activeRules.profile();
	const std::string key = p.worldId;
	if (key == trafficKey && traffic.configured()) return;
	trafficKey = key;
	// T2.exe: T2TrafficInfo(floorRect, 100, 500, 1000, 4) over the floor area.
	const int left = p.buildMinX == INT_MIN ? 0 : p.unitForX(p.buildMinX);
	const int right = p.buildMaxX == INT_MAX ? p.worldWidth : p.unitForX(p.buildMaxX);
	const int top = p.highestFloor == INT_MAX ? 0 : p.rowForFloor(p.highestFloor);
	const int bottom = p.rowForFloor(p.lowestFloor) + 1;
	traffic.configure(left, top, right, bottom);
}

void YootExtrasSystem::advance()
{
	if (!enabled()) return;
	configureTraffic();
	const long long now = Yoot::absoluteMinute(game->time.absolute);
	if (now == lastMinute) return;
	const bool first = lastMinute < 0;
	const long long prev = lastMinute;
	lastMinute = now;
	const int minute = nowMinuteOfDay(game);
	const long long hour = now / 60;
	if (!first && hour != lastHour) traffic.hourChanged(); // HourChanged 0x1006b640
	lastHour = hour;

	// The School's idles fall on every 8th minute: none is skipped.
	for (long long m = first || now - prev > 1440 || now < prev ? now : prev + 1; m <= now; ++m)
		schoolMinute(m, (int)(((m % 1440) + 1440) % 1440));
	for (Flight & f : flights)
		if (f.active) flightMinute(f, minute);
	if (maru.active) maruMinute(now, minute);
}

// ----------------------------------------------------------------- events

bool YootExtrasSystem::begin(const YootEventSpec & e, int index)
{
	if (e.type == "TOEV" || e.type == "DOEV")
	{
		Flight * f = nullptr;
		for (Flight & g : flights) if (g.index == index) f = &g;
		if (!f) { flights.push_back(Flight()); f = &flights.back(); f->index = index; }
		if (f->active) return true;
		if (!Yoot::decodeYootTonbi(e, f->def)) f->def = Yoot::builtinYootTonbi(e.type == "DOEV");
		f->endMin = e.endMin;
		f->active = true;
		f->run.start(f->def);
		LOG(INFO, "%s begins: %s flights every 128 minutes until %02d:%02d", e.type.c_str(),
		    e.type == "DOEV" ? "dolphin" : "black kite", e.endMin / 60, e.endMin % 60);
		return true;
	}
	if (e.type == "MAEV")
	{
		YootReggaeDef def;
		if (decodeYootReggae(e, def) && def.isReggae()) return false;
		if (maru.active) return true;
		Item::Item * t = pickMaruTarget(nullptr);
		if (!t) { LOG(DEBUG, "Mr. Maru: no tenant to visit"); return true; }
		maru = Maru();
		maru.active = true;
		maru.index = index;
		maru.endMin = e.endMin;
		maru.target = t;
		const long long now = Yoot::absoluteMinute(game->time.absolute);
		maru.arriveAt = now + 3 * std::max(1, std::abs(t->position.y));
		// With the visitor pool he walks (VIPL 223: silhouette 353 "Maru").
		maru.walker = game->eventSystem.people.spawnVip(1, 353, t);
		if (maru.walker) maru.arriveAt = -1;
		LOG(IMPORTANT, "Mr. Maru visits %s on floor %d", t->prototype->name.c_str(), t->position.y);
		return true;
	}
	return false;
}

void YootExtrasSystem::syncRunning(YootEventScheduler & scheduler) const
{
	for (const Flight & f : flights)
		if (f.index >= 0) scheduler.setRunning(f.index, f.active);
	if (maru.index >= 0) scheduler.setRunning(maru.index, maru.active);
}

// ------------------------------------------------------------ black kite

void YootExtrasSystem::flightMinute(Flight & f, int minute)
{
	// T2Tonbi::IsReady: no flight under the Tokyo rain / snow skies; Hawaii
	// and Kegon keep the base sky state, so it is always ready there.
	const bool ready = game->activeRules.profile().worldId != "tokyo" || !game->sky.rainyDay;
	for (int k = 0; k < 4 && f.active; ++k) // 4 Exec ticks per game minute
		if (f.run.tick(minute, f.endMin, ready, []() { return rand(); }))
			f.active = false;
}

sf::Texture * YootExtrasSystem::worldTexture(int id) const
{
	std::map<int, sf::Texture>::iterator it = textures.find(id);
	if (it == textures.end())
	{
		sf::Texture & tex = textures[id];
		YootWorld::Id w;
		if (parseYootWorldIdentifier(game->activeRules.profile().worldId, w))
			game->app.bitmaps.loadFromPlugin(yootWorldPluginFile(w), id, tex, sf::Color::White);
		it = textures.find(id);
	}
	return it->second.getSize().x ? &it->second : nullptr;
}

void YootExtrasSystem::drawFlight(sf::RenderTarget & target, const Flight & f) const
{
	if (!f.active || !f.run.visible() || f.def.minPict <= 0) return;
	sf::Texture * tex = worldTexture(f.def.minPict);
	if (!tex) return;
	// Frames are stacked vertically in bitmap minPict (OBJMAP rects).
	const int frames = std::max(1, f.def.maxPict - f.def.minPict + 1);
	const int fh = (int)tex->getSize().y / frames;
	const int frame = std::max(0, std::min(frames - 1, f.run.pattern() - f.def.minPict));
	const RulesProfile & p = game->activeRules.profile();
	sf::Sprite s(*tex);
	s.setTextureRect(sf::IntRect({0, frame * fh}, {(int)tex->getSize().x, fh}));
	s.setPosition({(float)(f.run.x() - p.unitOriginX * 8), (float)(f.run.y() - p.groundRow * 36)});
	target.draw(s);
	game->drawnSprites++;
}

void YootExtrasSystem::drawBehindTower(sf::RenderTarget & target) const
{
	if (!enabled()) return;
	for (const Flight & f : flights)
		if (f.def.layer <= 1) drawFlight(target, f);
}

void YootExtrasSystem::drawInFront(sf::RenderTarget & target) const
{
	if (!enabled()) return;
	for (const Flight & f : flights)
		if (f.def.layer > 1) drawFlight(target, f);
}

// --------------------------------------------------------------- Mr. Maru

Item::Item * YootExtrasSystem::pickMaruTarget(Item::Item * not_) const
{
	// FindTargetTenant 0x1003b660: a random lobby-connected tenant (category
	// filter 0 = any), not the current one.
	std::vector<Item::Item *> pool;
	for (Item::Item * item : game->items)
	{
		if (item == not_ || item->underConstruction || item->lobbyRoute.empty()) continue;
		if (item->canHaulPeople() || item->isLobby() || item->prototype->id == "floor" ||
		    item->prototype->id == "skybridge") continue;
		pool.push_back(item);
	}
	return pool.empty() ? nullptr : pool[rand() % pool.size()];
}

void YootExtrasSystem::maruMinute(long long now, int minute)
{
	PeopleSystem & people = game->eventSystem.people;
	if (maru.walker && !people.alive(maru.walker)) maru.walker = nullptr, maru.target = nullptr;
	if (maru.target && game->items.find(maru.target) == game->items.end()) maru.target = nullptr;
	if (maru.walker && maru.arriveAt < 0)
	{
		// Walking: he reaches the tenant and stands there (MaruReception).
		if (!maru.walker->posing && minute < maru.endMin && maru.target) return;
		if (maru.walker->posing) maru.arriveAt = now;
	}
	if (minute >= maru.endMin || !maru.target)
	{
		if (maru.walker) people.vipLeave(maru.walker, maru.target);
		maru.walker = nullptr;
		// State 5 past endTime: he heads for the exit (DeadVIP ends the event).
		LOG(IMPORTANT, "Mr. Maru leaves after %d visit(s)", maru.visits);
		maru.active = false;
		maru.target = nullptr;
		return;
	}
	if (now < maru.arriveAt) return;
	const int phase = Yoot::maruPhase((int)(now - maru.arriveAt));
	if (maru.walker)
	{
		// ChangeStyle(1) (sheet 354) while he stays; walk style 0 sitting,
		// 2 the howl of the dance.
		maru.walker->style = phase < 3 ? 1 : 0;
		maru.walker->walkStyle = phase == 1 ? 2 : 0;
	}
	if (phase == 1 && !maru.soundPlayed)
	{
		// State 3: "MR_Maru" (world WAVE 9003) while he dances on screen.
		maru.soundPlayed = true;
		if (!Sim::deterministic())
		{
			YootWorld::Id w;
			AbstractPlugin * plugin = parseYootWorldIdentifier(game->activeRules.profile().worldId, w)
				? game->app.pluginManager.get(yootWorldPluginFile(w)) : nullptr;
			const WindowsPEExecutable::Resource * wav = plugin && plugin->ensureLoaded()
				? plugin->getResource("WAVE", Yoot::kMaruSoundId) : nullptr;
			if (wav && !maruBuffer)
			{
				maruBuffer.reset(new sf::SoundBuffer());
				if (!maruBuffer->loadFromMemory(wav->data, wav->length)) maruBuffer.reset();
			}
			if (maruBuffer)
			{
				maruSound.reset(new sf::Sound(*maruBuffer));
				maruSound->play();
			}
		}
	}
	if (phase == 3)
	{
		maru.visits++;
		Item::Item * next = pickMaruTarget(maru.target);
		if (!next) next = maru.target;
		const int floors = std::max(1, std::abs(next->position.y - maru.target->position.y));
		if (maru.walker) people.vipGo(maru.walker, maru.target, next);
		maru.target = next;
		maru.arriveAt = maru.walker ? -1 : now + 3 * floors;
		maru.soundPlayed = false;
	}
}

// ----------------------------------------------------------------- school

void YootExtrasSystem::schoolMinute(long long now, int minute)
{
	const Game::ItemSet & s = itemsOf(game, "school");
	for (Item::Item * item : s)
	{
		Item::School * school = dynamic_cast<Item::School *>(item);
		if (!school || school->underConstruction) continue;
		const bool reachable = school->position.y == 0 || !school->lobbyRoute.empty();
		// With the visitor pool the 60 students are pool people (TMDF 1,
		// CollectEmployee) who walk in and out.
		PeopleSystem & people = game->eventSystem.people;
		const bool walk = people.enabled() && !people.pool.empty();
		if (!walk) school->model.arrive(now);
		if (minute % Yoot::kTenantIdleMinutes == 0)
		{
			const bool hiring = school->model.status == Yoot::SchoolModel::Hiring;
			school->model.idle(now, minute, school->openMinute(), school->closeMinute(),
			                   game->time.isHoliday(), reachable && (!walk || !hiring || people.hireBelong(school)), []() { return rand(); });
		}
		if (walk)
		{
			people.schoolTick(school, school->model.arrivals, school->model.inside, now);
			const int students = (int)people.belongOf(school).size();
			if (school->population != students) { school->population = students; game->populationNeedsUpdate = true; }
		}
		school->status = school->model.status;
		school->setPattern(school->model.pattern);
		if (school->model.inside > school->visitorsToday) school->visitorsToday = school->model.inside;
	}
}

// ------------------------------------------------------------- hot spring

void YootExtrasSystem::onBuilt(int topFloor)
{
	if (!enabled() || (game->yootSecretItems & 1)) return;
	const YootEventScheduler & sch = game->eventSystem.yootScheduler();
	const int idx = sch.find("HSEV");
	if (idx < 0) return;
	const YootEventSpec & e = sch.events()[idx];
	YootHotSpringDef def;
	const int threshold = decodeYootHotSpring(e, def) && def.a > 0 ? def.a : 100;
	if (!Yoot::hotSpringTriggers(false, e.levelBit, game->rating + 1, game->population, threshold, topFloor))
		return;
	// Dialog 8700, then FUN_00422080(1): doc+0x9c |= 1 loads the secret
	// plugins (the Kegon Spa). The original keeps this bit in the .bld
	// header (f9c); OpenSky saves it as yootSecretItems.
	game->yootSecretItems |= 1;
	game->activeRules.setSecretUnlocked(true);
	game->toolboxWindow.reload();
	game->askEventChoice("Hot Spring", Yoot::kHotSpringMessage, "OK", "", [](bool) {});
	game->timeWindow.showMessage("Spa was found underground. Spa can be placed.");
	LOG(IMPORTANT, "Hot spring found: the Kegon Spa is unlocked");
}

// ---------------------------------------------------------------- traffic

void YootExtrasSystem::bookRoute(const Route & route)
{
	if (!enabled() || !traffic.configured() || route.nodes.size() < 2) return;
	const RulesProfile & p = game->activeRules.profile();
	for (size_t i = 0; i + 1 < route.nodes.size(); ++i)
	{
		const Item::Item * a = route.nodes[i].item;
		const Item::Item * b = route.nodes[i + 1].item;
		if (!a || !b) continue;
		const int floor = route.nodes[i].toFloor;
		const int xa = a->position.x + a->size.x / 2;
		const int xb = b->position.x + b->size.x / 2;
		traffic.pass(p.rowForFloor(floor), p.unitForX(xa), p.unitForX(xb));
	}
}

int YootExtrasSystem::trafficRange(const Item::Item * item) const
{
	if (!item || !traffic.configured()) return 0;
	const RulesProfile & p = game->activeRules.profile();
	return traffic.range(p.rowForFloor(item->position.y), p.unitForX(item->position.x + item->size.x / 2));
}

// ------------------------------------------------------------ persistence

void YootExtrasSystem::encodeXML(tinyxml2::XMLPrinter & xml) const
{
	if (!enabled() || !traffic.configured()) return;
	const std::string t = traffic.encode();
	if (t.find(':') != std::string::npos) xml.PushAttribute("yootTraffic", t.c_str());
}

void YootExtrasSystem::decodeXML(tinyxml2::XMLElement & xml)
{
	if (!enabled()) return;
	configureTraffic();
	if (const char * t = xml.Attribute("yootTraffic")) traffic.decode(t);
}
