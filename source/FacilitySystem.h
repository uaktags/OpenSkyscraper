/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "YootFacilities.h"

#include <map>
#include <string>
#include <utility>

namespace OT
{
	class Person;
	namespace Item { class Item; class YootFacility; }

	/** YootTower trash, restrooms, colds and doctors (game-parity.md I2,
	 * I3, I5, P5, P6, R6; plan step 12).
	 *
	 * The rules are the pure functions in YootFacilities.h; this class
	 * applies them to the tower once per game minute and from the people
	 * hooks. Owned by EventSystem next to PowerSystem and driven from the
	 * YootTower dispatch; every entry point is a no-op outside YootTower.
	 *
	 * Simplifications (documented in game-parity.md):
	 * - a person who finds a restroom or a doctor is counted inside it for
	 *   the plugin's stay time but does not walk there and back;
	 * - garbage is delivered once per hour by every open restaurant that
	 *   has a reachable facility with room (T2Tenant::CollectCustomer from
	 *   the SearchRange tenants is not modelled per customer). */
	class FacilitySystem : public GameObject
	{
	public:
		FacilitySystem(Game * game);

		void reset();
		bool enabled() const;

		/// Per-frame: runs minuteTick once per new game minute.
		void advance();

		/// T2People::Idle case 1 (leaving the pool at the tower entrance):
		/// rolls the toilet need with GetToiletDenomi(grade).
		void onEnterTower(Person * person);
		/// T2Tenant::EnterTenant 0x1005e3a0: a person who needs a toilet
		/// and arrives at a tenant with IsNeedToilet searches a restroom.
		void onArrive(Person * person, Item::Item * destination);

		/// Inspector lines for facilities, restaurants and offices ("" if none).
		std::string describe(Item::Item * item);

		/// Toilet searches so far (found / failed), for diagnostics.
		int toiletVisits = 0;
		int toiletFailures = 0;

	private:
		int lastMinute;
		long long lastAbsMinute;
		bool trashWarned;
		int lastColdDay;
		size_t routeCacheItems;
		std::map<std::pair<Item::Item *, Item::Item *>, bool> routeCache;
		std::map<Item::Item *, Yoot::TrashComment> trashComments;
		std::map<Item::Item *, bool> needsDoctor;
		std::map<std::string, long long> messageLatch;

		int grade() const;
		const YootFacilityRules & rules() const;
		void minuteTick(int minute, long long absMinute);
		bool reachable(Item::Item * from, Item::Item * to);
		void expireOccupants(long long absMinute);
		void enter(Item::YootFacility * facility, long long absMinute);
		void trashTick(int minute);
		void deliverGarbage();
		void dailyColds();
		void callDoctors(long long absMinute);
		void message(const std::string & text, long long absMinute, int everyMinutes);
	};
}
