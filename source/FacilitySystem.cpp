/* Copyright (c) 2026 OpenSkyscraper Project */
/* YootTower trash (TrashFacility.t2p), restrooms (T2People::SearchToilet),
 * colds and doctors (Office.t2p / Doctor's.t2p). game-parity.md I2, I3,
 * I5, P5, P6, R6. */
#include "FacilitySystem.h"

#include "Application.h"
#include "Game.h"
#include "Item/Item.h"
#include "Item/Office.h"
#include "Item/YootMiscItems.h"
#include "Item/YootFacilityItems.h"
#include "Logger.h"
#include "PeopleSystem.h"
#include "Person.h"
#include "YootCatalog.h"
#include "YootRules.h"

#include <climits>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>

using namespace OT;

namespace
{
	const Game::ItemSet & itemsOf(Game * game, const char * id)
	{
		static const Game::ItemSet none;
		Game::ItemSetByString::const_iterator it = game->itemsByType.find(id);
		return it == game->itemsByType.end() ? none : it->second;
	}

	bool isRestaurant(const Item::Item * item)
	{
		// ToolDef category 50 (TrashFacility.t2p SearchRange FUN_1000bf00).
		if (item->prototype->yoot) return item->prototype->yoot->category == 50;
		const std::string & id = item->prototype->id;
		return id == "fastfood" || id == "restaurant";
	}

	bool built(const Item::Item * item) { return item && !item->underConstruction; }
}

FacilitySystem::FacilitySystem(Game * game)
: GameObject(game)
{
	reset();
}

void FacilitySystem::reset()
{
	lastMinute = -1;
	lastAbsMinute = -1;
	trashWarned = false;
	lastColdDay = -1;
	routeCacheItems = 0;
	routeCache.clear();
	trashComments.clear();
	needsDoctor.clear();
	messageLatch.clear();
	toiletVisits = toiletFailures = 0;
}

bool FacilitySystem::enabled() const
{
	return game && game->activeRules.profile().yootWorld;
}

int FacilitySystem::grade() const
{
	return game->rating + 1; // doc+0x8c is 1-based
}

const YootFacilityRules & FacilitySystem::rules() const
{
	return game->activeRules.profile().facilities;
}

void FacilitySystem::advance()
{
	if (!enabled()) return;
	const int minute = std::max(0, std::min(1439, (int)std::floor(game->time.hour * 60.0)));
	if (minute == lastMinute) return;
	lastMinute = minute;
	minuteTick(minute, Yoot::absoluteMinute(game->time.absolute));
}

void FacilitySystem::minuteTick(int minute, long long absMinute)
{
	if (game->items.size() != routeCacheItems || minute % 60 == 0) {
		routeCache.clear();
		routeCacheItems = game->items.size();
	}
	expireOccupants(absMinute);
	trashTick(minute);
	if (minute % 60 == 0) deliverGarbage();

	// Office.t2p idle at 00:00-00:08: last day's colds cost estimate, then
	// today's colds are rolled (not on holidays).
	const int day = (int)std::floor(game->time.absolute);
	if (minute < 8 && day != lastColdDay) {
		lastColdDay = day;
		dailyColds();
	}
	if (Yoot::doctorCallMinute(minute) && minute % 60 == 0) callDoctors(absMinute);

	// Doctor's.t2p ChangeStatusOnIdle: closed outside its hours.
	const Game::ItemSet & docs = itemsOf(game, "doctor");
	for (Game::ItemSet::const_iterator i = docs.begin(); i != docs.end(); ++i) {
		Item::DoctorsOffice * d = dynamic_cast<Item::DoctorsOffice *>(*i);
		if (!d || !built(d)) continue;
		if (!d->isOpen(minute)) { d->leaveAt.clear(); d->setPattern(0); }
		else if (d->pattern == 0) d->setPattern(1);
	}
	lastAbsMinute = absMinute;
}

bool FacilitySystem::reachable(Item::Item * from, Item::Item * to)
{
	if (!from || !to) return false;
	if (from->position.y == to->position.y) return true;
	std::pair<Item::Item *, Item::Item *> key(from, to);
	std::map<std::pair<Item::Item *, Item::Item *>, bool>::const_iterator c = routeCache.find(key);
	if (c != routeCache.end()) return c->second;
	// T2RouteNavigator::CheckRoute: a route exists (the score cap is not
	// modelled; OpenSky routes are already bounded by the A* search).
	const bool ok = !game->findRoute(from, to).empty();
	routeCache[key] = ok;
	return ok;
}

void FacilitySystem::expireOccupants(long long absMinute)
{
	static const char * kinds[] = {"restroom", "doctor"};
	for (const char * kind : kinds) {
		const bool doctor = std::string(kind) == "doctor";
		const Game::ItemSet & set = itemsOf(game, kind);
		for (Game::ItemSet::const_iterator i = set.begin(); i != set.end(); ++i) {
			Item::YootFacility * f = dynamic_cast<Item::YootFacility *>(*i);
			if (!f) continue;
			for (size_t k = 0; k < f->leaveAt.size();) {
				if (f->leaveAt[k] > absMinute) { ++k; continue; }
				f->leaveAt.erase(f->leaveAt.begin() + k);
				// LeaveTenant FUN_1000ce20 / FUN_1000bc90.
				f->setPattern(doctor
					? Yoot::doctorPattern(f->pattern, f->users(), f->capacity(), false)
					: Yoot::restroomPattern(f->pattern, f->users(), f->capacity(), false));
			}
		}
	}
}

void FacilitySystem::enter(Item::YootFacility * f, long long absMinute)
{
	f->leaveAt.push_back(absMinute + f->stayMinutes());
	const bool doctor = f->prototype->id == "doctor";
	f->setPattern(doctor
		? Yoot::doctorPattern(f->pattern, f->users(), f->capacity(), true)
		: Yoot::restroomPattern(f->pattern, f->users(), f->capacity(), true));
}

void FacilitySystem::trashTick(int minute)
{
	const Game::ItemSet & facilities = itemsOf(game, "trashfacility");
	for (Game::ItemSet::const_iterator i = facilities.begin(); i != facilities.end(); ++i) {
		Item::TrashFacility * t = dynamic_cast<Item::TrashFacility *>(*i);
		if (!t || !built(t)) continue;
		int open = 450, close = 420; // TrashFacility.t2p TNDF +0x174 / +0x178
		if (t->prototype->yoot && t->prototype->yoot->openMinute > 0) {
			open = t->prototype->yoot->openMinute;
			close = t->prototype->yoot->closeMinute;
		}
		if (t->model.idle(minute, open, close)) t->setPattern(t->model.pattern);
	}

	// DefIdleProc FUN_1000bde0: runs only while the plugin is loaded, i.e.
	// from its grade on, and only when no facility exists.
	if (!Yoot::trashWarningMinute(minute)) { trashWarned = false; return; }
	if (trashWarned) return;
	const int minRating = game->activeRules.minRatingToBuild("trashfacility");
	if (minRating == BuildRules::kUnavailable || minRating > game->rating) return;
	if (!facilities.empty()) return;
	trashWarned = true;
	game->timeWindow.showMessage(Yoot::kTrashWarning);
	LOG(IMPORTANT, "%s (%02d:%02d)", Yoot::kTrashWarning, minute / 60, minute % 60);
}

void FacilitySystem::deliverGarbage()
{
	trashComments.clear();
	if (!Yoot::isDustOn(rules(), grade())) return;
	const Game::ItemSet & facilities = itemsOf(game, "trashfacility");
	for (Game::ItemSet::const_iterator r = game->items.begin(); r != game->items.end(); ++r) {
		Item::Item * restaurant = *r;
		if (!built(restaurant) || !isRestaurant(restaurant)) continue;
		Yoot::TrashComment comment = facilities.empty()
			? Yoot::TrashComment::NoFacility : Yoot::TrashComment::NotConnected;
		for (Game::ItemSet::const_iterator f = facilities.begin(); f != facilities.end(); ++f) {
			Item::TrashFacility * t = dynamic_cast<Item::TrashFacility *>(*f);
			if (!t || !built(t) || !reachable(restaurant, t)) continue;
			if (t->model.deliver()) {
				t->setPattern(t->model.pattern);
				comment = Yoot::TrashComment::None;
				break;
			}
			comment = Yoot::TrashComment::Full;
		}
		if (comment != Yoot::TrashComment::None) trashComments[restaurant] = comment;
	}
}

void FacilitySystem::dailyColds()
{
	const bool holiday = game->time.isHoliday();
	const int denomi = Yoot::coldDenomi(rules(), grade(), game->time.dayOfYear() + 1);
	const Game::ItemSet & offices = itemsOf(game, "office");
	for (Game::ItemSet::const_iterator i = offices.begin(); i != offices.end(); ++i) {
		Item::Office * office = dynamic_cast<Item::Office *>(*i);
		if (!office || !built(office)) continue;
		const std::vector<Person *> workers = office->getWorkerPeople(INT_MAX);
		for (Person * w : workers) {
			// FUN_1000b8b0: still sick at midnight.
			if (w->yootFlags & Yoot::kPeopleSick) {
				w->yootFlags &= ~Yoot::kPeopleSick;
				w->yootEstimate = Yoot::incEstimate(w->yootEstimate, -Yoot::kSickEstimate);
			}
			// FUN_1000b850 on working days.
			if (!holiday && Yoot::rollDenomi(denomi, std::rand()))
				w->yootFlags |= Yoot::kPeopleSick;
		}
	}
	// School.t2p 0x1000b8b0 / 0x1000b850 (status > 10): the same for its
	// students, whose flags stay with the school while they are home (the
	// still-sick lose 50 estimate in the original; OpenSky keeps no
	// estimate for a student out of the tower).
	const Game::ItemSet & schools = itemsOf(game, "school");
	for (Game::ItemSet::const_iterator i = schools.begin(); i != schools.end(); ++i) {
		Item::School * school = dynamic_cast<Item::School *>(*i);
		if (!school || !built(school) || school->model.status <= Yoot::SchoolModel::Hiring) continue;
		for (int & f : game->eventSystem.people.belongFlags(school)) {
			f &= ~Yoot::kPeopleSick;
			if (!holiday && Yoot::rollDenomi(denomi, std::rand())) f |= Yoot::kPeopleSick;
		}
	}
}

void FacilitySystem::callDoctors(long long absMinute)
{
	needsDoctor.clear();
	const Game::ItemSet & offices = itemsOf(game, "office");
	const Game::ItemSet & docs = itemsOf(game, "doctor");
	const int minute = lastMinute;
	// School.t2p 0x1000b900 (status > 11): up to 5 sick students inside go
	// to a reachable, open doctor with room (0x1000ba80) and are treated.
	const Game::ItemSet & schools = itemsOf(game, "school");
	for (Game::ItemSet::const_iterator i = schools.begin(); i != schools.end(); ++i) {
		Item::School * school = dynamic_cast<Item::School *>(*i);
		if (!school || !built(school) || school->model.status <= Yoot::SchoolModel::Closed) continue;
		std::vector<int> & flags = game->eventSystem.people.belongFlags(school);
		int sent = 0;
		for (YootVisitor * v : game->eventSystem.people.visitorsIn(school)) {
			if (sent >= 5) break;
			if (v->belongIndex < 0 || v->belongIndex >= (int)flags.size() || !(flags[v->belongIndex] & Yoot::kPeopleSick)) continue;
			std::vector<Item::DoctorsOffice *> list;
			std::vector<Yoot::DoctorCandidate> candidates;
			for (Game::ItemSet::const_iterator d = docs.begin(); d != docs.end(); ++d) {
				Item::DoctorsOffice * doc = dynamic_cast<Item::DoctorsOffice *>(*d);
				if (!doc || !built(doc)) continue;
				Yoot::DoctorCandidate c;
				c.reachable = reachable(school, doc);
				c.open = doc->isOpen(minute);
				c.patients = doc->users();
				c.capacity = doc->capacity();
				list.push_back(doc);
				candidates.push_back(c);
			}
			const int found = Yoot::searchDoctor(candidates);
			if (found < 0) { needsDoctor[school] = true; break; }
			enter(list[found], absMinute);
			flags[v->belongIndex] &= ~Yoot::kPeopleSick;
			++sent;
		}
	}
	for (Game::ItemSet::const_iterator i = offices.begin(); i != offices.end(); ++i) {
		Item::Office * office = dynamic_cast<Item::Office *>(*i);
		if (!office || !built(office)) continue;
		// FUN_1000b900 with param_3 = 1: one sick employee in the office.
		Person * patient = nullptr;
		const std::vector<Person *> workers = office->getWorkerPeople(INT_MAX);
		for (Person * w : workers)
			if ((w->yootFlags & Yoot::kPeopleSick) && w->at == office) { patient = w; break; }
		if (!patient) continue;

		std::vector<Item::DoctorsOffice *> list;
		std::vector<Yoot::DoctorCandidate> candidates;
		for (Game::ItemSet::const_iterator d = docs.begin(); d != docs.end(); ++d) {
			Item::DoctorsOffice * doc = dynamic_cast<Item::DoctorsOffice *>(*d);
			if (!doc || !built(doc)) continue;
			Yoot::DoctorCandidate c;
			c.reachable = reachable(office, doc);
			c.open = doc->isOpen(minute);
			c.patients = doc->users();
			c.capacity = doc->capacity();
			list.push_back(doc);
			candidates.push_back(c);
		}
		const int found = Yoot::searchDoctor(candidates);
		if (found >= 0) {
			enter(list[found], absMinute);
			patient->yootFlags &= ~Yoot::kPeopleSick; // treated
		} else {
			// The plugin's balloon (LoadString 11001/11002) has no string in
			// the shipped game; the tenant comment 1034 stands for it here.
			needsDoctor[office] = true;
		}
	}
}

void FacilitySystem::onEnterTower(Person * person)
{
	if (!enabled() || !person) return;
	const int denomi = Yoot::toiletDenomi(rules(), grade());
	if (Yoot::rollDenomi(denomi, std::rand()))
		person->yootFlags |= Yoot::kPeopleNeedsToilet;
}

void FacilitySystem::onArrive(Person * person, Item::Item * destination)
{
	if (!enabled() || !person || !destination) return;
	if (!(person->yootFlags & Yoot::kPeopleNeedsToilet)) return;
	if (!destination->prototype->yoot || !destination->prototype->yoot->needsToilet()) return;

	const long long absMinute = Yoot::absoluteMinute(game->time.absolute);
	std::vector<Item::YootFacility *> list;
	std::vector<Yoot::RestroomCandidate> candidates;
	const Game::ItemSet & rooms = itemsOf(game, "restroom");
	for (Game::ItemSet::const_iterator i = rooms.begin(); i != rooms.end(); ++i) {
		Item::YootFacility * room = dynamic_cast<Item::YootFacility *>(*i);
		if (!room || !built(room)) continue;
		Yoot::RestroomCandidate c;
		c.reachable = reachable(destination, room);
		c.users = room->users();
		c.capacity = room->capacity();
		list.push_back(room);
		candidates.push_back(c);
	}
	int index = -1;
	const Yoot::ToiletResult result = Yoot::searchToilet(candidates, index);
	person->yootFlags &= ~Yoot::kPeopleNeedsToilet;
	if (result == Yoot::ToiletResult::Found) {
		enter(list[index], absMinute);
		++toiletVisits;
		return;
	}
	// T2People::SearchToilet: balloon, flag cleared, -30 estimate.
	++toiletFailures;
	person->yootEstimate = Yoot::incEstimate(person->yootEstimate, -Yoot::kToiletFailEstimate);
	message(Yoot::toiletFailureMessage(result), absMinute, 60);
}

void FacilitySystem::message(const std::string & text, long long absMinute, int everyMinutes)
{
	// The original shows a balloon over the person; one status-bar line per
	// text and hour keeps that readable.
	std::map<std::string, long long>::iterator it = messageLatch.find(text);
	if (it != messageLatch.end() && absMinute - it->second < everyMinutes) return;
	messageLatch[text] = absMinute;
	game->timeWindow.showMessage(text);
}

std::string FacilitySystem::describe(Item::Item * item)
{
	if (!enabled() || !item) return std::string();
	std::ostringstream s;
	if (Item::TrashFacility * t = dynamic_cast<Item::TrashFacility *>(item)) {
		s << "\nGarbage:     " << t->model.garbage << "/" << t->model.capacity;
		if (t->model.status == Yoot::TrashFacilityModel::kEmptying) s << " (emptying)";
		s << "\n";
	} else if (Item::YootFacility * f = dynamic_cast<Item::YootFacility *>(item)) {
		s << "\nIn use:      " << f->users() << "/" << f->capacity() << "\n";
		if (item->prototype->id == "restroom")
			s << "Visits:      " << toiletVisits << " (failed " << toiletFailures << ")\n";
	}
	std::map<Item::Item *, Yoot::TrashComment>::const_iterator c = trashComments.find(item);
	if (c != trashComments.end()) s << "\n" << Yoot::trashCommentText(c->second) << "\n";
	if (Item::Office * office = dynamic_cast<Item::Office *>(item)) {
		int sick = 0;
		const std::vector<Person *> workers = office->getWorkerPeople(INT_MAX);
		for (Person * w : workers) if (w->yootFlags & Yoot::kPeopleSick) ++sick;
		if (sick) s << "\nSick workers: " << sick << "\n";
		if (needsDoctor.count(item)) s << Yoot::kNeedsDoctor << "\n";
	}
	return s.str();
}
