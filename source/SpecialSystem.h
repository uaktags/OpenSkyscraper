/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include "GameObject.h"
#include "YootSpecials.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace OT
{
	class Person;
	namespace Item { class Item; class Hotel; class FrontDesk; class Housekeeping; class ParkingLot; }

	/** YootTower special items, part A: Front Desk + Housekeeping, parking
	 * and the fire department (game-parity.md W4, E7, I4-I6; plan step 14).
	 *
	 * The rules are the pure functions in YootSpecials.h; this class applies
	 * them once per game minute and from the item hooks (Hotel, Office).
	 * Owned by EventSystem next to FacilitySystem and driven from the
	 * YootTower dispatch; every entry point is a no-op outside YootTower.
	 *
	 * Simplifications (documented in game-parity.md):
	 * - guests walk straight to their room; the check-in is booked on the
	 *   room's linked desk when the first guest of the stay arrives (the
	 *   original queues them at the desk first, FUN_10013ce0);
	 * - rooms are (re)linked every game hour and when a desk is built
	 *   instead of only when the desk's staff arrive (FUN_10013b70);
	 * - one maid trip covers up to three dirty rooms, each drawn as its own
	 *   housekeeper walking from the Housekeeping item.
 * Not shown: "A Front is required..." and "The residents are requesting
 * parking facilities." live in vtable slot 50 of Front.t2p / Parking.t2p,
 * past T2TenantDef's 50 virtuals, so the engine never calls them (the
 * oracle never showed the parking one at grade 3 either). */
	class SpecialSystem : public GameObject
	{
	public:
		SpecialSystem(Game * game);

		void reset();
		bool enabled() const;
		/// Per-frame: runs minuteTick once per new game minute.
		void advance();

		// ---- hotel (Hotel.cpp hooks) ----
		/// W4: a room whose plugin depends on the Front Desk gets guests
		/// only while a desk has linked it.
		bool roomMayTakeGuests(Item::Hotel * room);
		/// E7: the first guest of a stay arrived; books the room's inMoney
		/// on its desk (paid at 12:00). Unlinked rooms that need no desk
		/// (Kegon) pay at once.
		void onCheckIn(Item::Hotel * room);
		Item::FrontDesk * deskOf(Item::Item * room) const;

		// ---- parking (Office.cpp / Hotel.cpp hooks) ----
		/// T2People::MoveToParking: a driver bound for `destination` takes a
		/// free slot in a reachable lot. Returns false (and costs 100
		/// estimate once parking exists) when none is free.
		bool driveIn(Person * driver, Item::Item * destination);
		/// The driver leaves the tower: frees the slot.
		void driveOut(Person * driver);

		// ---- fire (EventSystem::deployFireGuards) ----
		/// CallFireman: guards per registry-5 source (security rooms then
		/// fire departments, list order), 6 in total.
		std::vector<std::pair<Item::Item *, int> > firemen();

		/// Inspector lines ("" if none).
		std::string describe(Item::Item * item);
		/// Front.t2p T2DLG 1000 "Room Rate:" popups (7154/7157/7158, one per
		/// room type tab): labels and ids "type:selection" (1-based).
		std::vector<std::pair<std::string, std::string> > rateChoices(Item::FrontDesk * desk) const;
		/// Stores the selection in the desk's refcon (+0x16[type]).
		bool chooseRate(Item::FrontDesk * desk, const std::string & id);
		/// The price selection (0-based inMoney index) of a room: its desk's
		/// popup for the room type, else -1 (Game falls back to the default).
		int roomSelection(const Item::Item * room) const;

		long long hotelIncomePaid = 0; ///< dollars, for tests and diagnostics

	private:
		int lastMinute, prevMinute;
		long long lastAbsMinute;
		size_t lastItemCount;
		std::map<Item::Item *, Item::FrontDesk *> roomDesk;
		std::map<Person *, Item::ParkingLot *> parked;
		struct Trip { Item::Housekeeping * from; std::vector<Item::Hotel *> rooms; };
		std::vector<Trip> trips;
		std::set<Item::Hotel *> beingCleaned;
		std::map<std::pair<std::pair<Item::Item *, Item::Item *>, bool>, bool> routeCache;

		void minuteTick(int minute);
		/// The clock passed minute-of-day `first` since the previous tick
		/// (fast speeds skip minutes; the tenant windows are 8 wide).
		bool crossed(int first) const;
		bool reachable(Item::Item * from, Item::Item * to, bool service = false);
		bool exists(Item::Item * item, const char * kind) const;
		void purge();
		void linkRooms();
		void deskTick(int minute);
		void housekeepingTick(int minute);
		void parkingTick();
	};
}
