#pragma once
#include <vector>
#include "../Sprite.h"
#include "Item.h"

namespace OT {
	namespace Item {
		// One 4-cell bay; quota accounting retains every driver, including double parking.
		class Parking : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Parking);
			OT_ITEM_PROTOTYPE(Parking) {
				p->id    = "parking";
				p->name  = "Parking";
				p->price = 3000;
				p->size  = int2(4, 1);
				// Authentic toolbox cell: the "P" sign (17). The old value
				// 20 pointed at the church dome.
				p->icon  = 17;
			}
			virtual ~Parking();
			virtual void evacuateForDisaster() override;

			/// Driving population a parked car's driver belongs to. The EXE
			/// admits exactly two categories (ParkingT SetupGateCount):
			/// office workers and hotel-suite guests. The quota table's
			/// copy-paste bug (ORIGINAL-BUGS B1: slot 4 set twice, condo slot
			/// 9 never) is harmless because only these two drive.
			enum CarCategory {
				kNoCar    = 0,
				kOfficeCar = 1,
				kSuiteCar  = 2
			};

			virtual void init();
			virtual void encodeXML(tinyxml2::XMLPrinter &xml);
			virtual void decodeXML(tinyxml2::XMLElement &xml);
			virtual void advance(double dt);

			/// Quarterly upkeep per parking RAMP (maintenance table 0x3EA
			/// item 44 = 10000 x $100). The quarterly settle sweep calls this
			/// once per ramp item; spaces and cars cost nothing.
			static const int kRampUpkeepPerQuarter;
			static int rampUpkeepPerQuarter() { return kRampUpkeepPerQuarter; }

			/// Spaces on this item: one per 4-cell bay.
			int totalSpaces() const;
			/// Bays currently holding a car.
			int usedSpaces() const;
			/// Bays usable for parking (on the B1 ramp chain, reachable).
			int usableSpaces() const;
			/// True iff a free usable bay exists.
			bool hasSpace() const;
			/// Claim the first free usable bay in fill (ordinal) order.
			/// `driver` (optional) tags the bay so it cannot be double-
			/// assigned (B2 divergence) and can be released precisely;
			/// `cat` counts the car against that category's quota.
			/// @return true if a bay was claimed.
			bool assignSpace(Person * driver = NULL, CarCategory cat = kOfficeCar);
			/// Release the bay held by `driver`, or - for legacy callers
			/// that pass no driver - the lowest occupied bay.
			void freeSpace(Person * driver = NULL);

			/// --- Tower-wide queries (Wave E: suite/VIP cancel-on-no-park) ---
			/// Total usable parking spaces across every Parking item.
			static int usableSpaceCount(Game * game);
			/// Admission quota PER CATEGORY: 2 x usable spaces
			/// (ParkingT SetupGateCount; double-parking is real in the EXE).
			static int carQuota(Game * game, CarCategory cat = kOfficeCar);
			static Parking *claim(Game*, Item *destination, Person *driver, CarCategory);
			static void recomputeAllChains(Game *game);
			/// Cars currently parked in category `cat`; kNoCar sums both.
			static int carsParked(Game * game, CarCategory cat = kNoCar);
			/// True iff a driver of `cat` may park right now: the category
			/// quota has room AND a free usable bay exists. A suite guest
			/// who cannot park cancels the stay (and voids an armed VIP
			/// visit) - that integration consumes this query.
			static bool canPark(Game * game, CarCategory cat);

			/// A destroyed person leaves every driver map (no dangling owners).
			static void forgetDriver(Person * driver);
			/// After a load, re-bind saved cars to their drivers (F4).
			static void relinkAll(Game * game);
			/// The category a given driver is parked under (kNoCar if none).
			CarCategory driverCategory(Person * driver) const;

			/// Per-bay state, maintained by the chain sweep and the
			/// assign/free calls.
			struct Bay {
				bool usable;          ///< on the B1 ramp chain + open drive path
				bool occupied;        ///< a car sits here
				Person * person;      ///< driver handle; non-NULL blocks re-assign (B2)
				CarCategory category; ///< which quota this car counts against
				int variant;          ///< sheet frame 2..14 (2 + id % 13, stable per space)
				int ordinal;          ///< tower-wide fill order among usable bays
				Bay() : usable(false), occupied(false), person(NULL),
				        category(kNoCar), variant(0), ordinal(0) {}
			};
			const std::vector<Bay> & bays() const { return bayList; }

			bool spriteNeedsUpdate;
			void updateSprite();

		private:
			std::vector<Bay> bayList;
			std::map<Person*, CarCategory> drivers;
			/// Saved cars awaiting relink: owner item cell, member, category.
			struct PendingCar { int x, y, member; CarCategory category; };
			std::vector<PendingCar> pendingCars;
			std::vector<Sprite> baySprites;
			bool wasUnderConstruction;

			/// (Re)derive the bay list from the item size; assigns each bay
			/// its deterministic car variant (2 + id % 13).
			void ensureBays();
			/// (Re)build the per-bay sprite set from the merged sheet.
			void rebuildBaySprites();
			/// Rebuild the ramp chain and recompute every Parking item's
			/// bay usability + ordinals (CheckAllParking sweep). Runs from
			/// advance() on layout changes and on the sim-hour tick.
		};
	}
}
