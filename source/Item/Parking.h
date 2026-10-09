#pragma once
#include <vector>
#include "../Sprite.h"
#include "Item.h"

namespace OT {
	namespace Item {
		/** Parking structure item (EXE "Parking Space", sheets 0x86A8+0x86A9).
		 *
		 * Each 4-cell (32 px) horizontal bay is ONE parking space, drawn from
		 * the merged 15-frame sheet "simtower/parking/space": frame 0 = empty
		 * bay, frame 1 = red X (space off the ramp chain), frames 2-14 = the
		 * 13 parked-car variants (per-space deterministic pick 2 + id%13,
		 * bays filled in ordinal order).
		 *
		 * Usability (ParkingT CheckAllParking): a bay is usable iff its floor
		 * is underground and ramp-chained to B1 - ramps stacked at the same x
		 * on consecutive basement floors, anchored at B1 - and the drive path
		 * from the ramp to the bay is not cut by another item or a >= 4-cell
		 * bare gap. Recomputed on layout changes and hourly (the EXE sweeps
		 * daily at 7AM; the finer cadence also catches construction
		 * completions, which the EXE handles by recomputing on build).
		 *
		 * Cars are admitted against the EXE per-category quota of 2 x usable
		 * spaces (office drivers and hotel-suite guests each get the full
		 * quota; ParkingT SetupGateCount). Parking earns $0 per car
		 * (InParkingCar 1198:031a makes zero MoneyT calls) and only RAMP
		 * items cost upkeep ($10k/quarter, rampUpkeepPerQuarter()).
		 *
		 * Divergence (ORIGINAL-BUGS B2): the EXE's InParkingCar never checks
		 * whether a space already holds a car, so two cars can share one
		 * physical space. We refuse that - an occupied bay is never
		 * assignable - so our physical capacity is one car per bay even
		 * though the admission quota stays the EXE's 2 x usable.
		 */
		class Parking : public Item
		{
		public:
			OT_ITEM_CONSTRUCTOR(Parking);
			OT_ITEM_PROTOTYPE(Parking) {
				p->id    = "parking";
				p->name  = "Parking";
				p->price = 3000;
				p->size  = int2(8, 1);
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
			static int carQuota(Game * game);
			/// Cars currently parked in category `cat`; kNoCar sums both.
			static int carsParked(Game * game, CarCategory cat = kNoCar);
			/// True iff a driver of `cat` may park right now: the category
			/// quota has room AND a free usable bay exists. A suite guest
			/// who cannot park cancels the stay (and voids an armed VIP
			/// visit) - that integration consumes this query.
			static bool canPark(Game * game, CarCategory cat);

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
			static void recomputeAllChains(Game * game);
		};
	}
}
