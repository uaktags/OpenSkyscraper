/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include "../YootSpecials.h"
#include "YootCondo.h"
#include "YootFacilityItems.h"

namespace OT {
	namespace Item {
		/// Front.t2p (RFront.t2p in Kegon): ATTR NORF, grade 2, TNDF 15x1,
		/// open 06:00, close 01:00, 3 staff, 4 view patterns. Links the
		/// guest rooms in its route range (W4) and pays their income at
		/// 12:00 through its ledger (E7). SpecialSystem drives it.
		class FrontDesk : public YootFacility
		{
		public:
			FrontDesk(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(FrontDesk) {
				p->id = "frontdesk";
				p->name = "Front Desk";
				p->price = 100000;
				p->size = int2(15, 1);
				p->icon = 18;
			}
			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			Yoot::FrontDeskLedger ledger;
			int linkedRooms = 0;
			/// A guest joins the check-in queue (PeopleSystem::deskEnter).
			virtual void addPerson(Person * p);
			int openMinute() const;
			int closeMinute() const;
			bool isOpen(int minuteOfDay) const;

		protected:
			const char * defaultPlugin() const { return "Front.t2p"; }
		};


		/// Parking.t2p variation 1, "Parking Gate" ($50,000, $10,000/Q):
		/// surface access, 16 units wide (OBJMAP 1000 Csr2 128 px), its
		/// bottom row on B1 for the first gate (Parking.t2p idle
		/// FUN_1000bd20: rect.bottom == groundLine + 1), stacking down.
		class ParkingGate : public YootFacility
		{
		public:
			ParkingGate(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(ParkingGate) {
				p->id = "parkinggate";
				p->name = "Parking Gate";
				p->price = 50000;
				p->size = int2(16, 2);
				p->icon = 17;
			}
			virtual void init();
			/// OBJMAP 1000 sorts the 14 lot patterns (100-113) before the
			/// gate's 1100-1102, so gate pattern n is view band 14 + n.
			static const int kGateBand = 14;
			void setGatePattern(int p) { setPattern(kGateBand + p); }
		protected:
			const char * defaultPlugin() const { return "Parking.t2p"; }
		};

		/// Parking.t2p variation 0, "Parking Lot" ($5,000): 4 units, next to
		/// a gate or another lot, up to 5 drivers (TNDF belongCapacity).
		/// Patterns 100-113: empty, then the parked car (id % 13 + 1).
		class ParkingLot : public YootFacility
		{
		public:
			ParkingLot(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(ParkingLot) {
				p->id = "parkinglot";
				p->name = "Parking Lot";
				p->price = 5000;
				p->size = int2(4, 1);
				p->icon = 17;
			}
			virtual void init();
			virtual void decodeXML(tinyxml2::XMLElement & xml);
			Yoot::ParkingLotModel model;
			/// Drivers are transient (not saved), like the facility users.
			std::vector<Person *> drivers;
			bool park(Person * driver);
			bool leave(Person * driver);
		protected:
			const char * defaultPlugin() const { return "Parking.t2p"; }
		};

		/// FireDepartment.t2p (Kegon only, grade 4): TNDF 12x1, registry 5
		/// like the Security Room, 8 firemen on day (05:00-17:00) and night
		/// shifts (ChangeStatusOnIdle FUN_1000bd40).
		class FireDepartment : public YootFacility
		{
		public:
			FireDepartment(Game * game, AbstractPrototype * prototype) : YootFacility(game, prototype) {}
			OT_ITEM_PROTOTYPE(FireDepartment) {
				p->id = "firedept";
				p->name = "Fire Department";
				p->price = 30000;
				p->size = int2(12, 1);
				p->icon = 18;
			}
			int firemen() const;
		protected:
			const char * defaultPlugin() const { return "FireDepartment.t2p"; }
		};

		/// Apartment.t2p (Tokyo, grade 1): a rented 4x1 home for one
		/// resident. Like the office it pays inMoney once when let and
		/// 3 x inMoney every quarter (oracle, game-parity.md E4); the
		/// 1-300 residential estimate decides whether it stays let.
		class Apartment : public YootCondo
		{
		public:
			Apartment(Game * game, AbstractPrototype * prototype) : YootCondo(game, prototype) {}
			OT_ITEM_PROTOTYPE(Apartment) {
				p->id = "apartment";
				p->name = "Apartment";
				p->price = 20000;
				p->size = int2(4, 1);
				p->icon = 24;
			}
		protected:
			virtual const char * condoPlugin() const;
			virtual void moveIn();
			virtual void moveOut();
			virtual int residentsToCreate() const { return 1; }
			virtual bool hasFamilies() const { return false; }
		};
	}
}
