/* Copyright (c) 2026 OpenSkyscraper Project */
#include "../Game.h"
#include "../PeopleSystem.h"
#include "../YootCatalog.h"
#include "YootSpecialItems.h"

#include <algorithm>
#include <cstdint>

using namespace OT;
using namespace OT::Item;

// ---------------------------------------------------------------- front desk

void FrontDesk::addPerson(Person * p)
{
	YootFacility::addPerson(p);
	if (YootVisitor * v = dynamic_cast<YootVisitor *>(p)) game->eventSystem.people.deskEnter(this, v);
}

int FrontDesk::openMinute() const
{
	if (prototype->yoot && (prototype->yoot->openMinute || prototype->yoot->closeMinute))
		return prototype->yoot->openMinute;
	return 360; // Front.t2p TNDF +0x174
}

int FrontDesk::closeMinute() const
{
	if (prototype->yoot && (prototype->yoot->openMinute || prototype->yoot->closeMinute))
		return prototype->yoot->closeMinute;
	return 60; // +0x178: 01:00, after midnight
}

bool FrontDesk::isOpen(int minute) const
{
	const int open = openMinute(), close = closeMinute();
	if (open <= close) return minute >= open && minute < close;
	return minute >= open || minute < close; // open across midnight
}

void FrontDesk::encodeXML(tinyxml2::XMLPrinter & xml)
{
	YootFacility::encodeXML(xml);
	// The refcon table (Front.t2p FUN_10015840 saves 3 x {selection, accrued}).
	for (int k = 0; k < 3; ++k) {
		char name[16];
		snprintf(name, sizeof(name), "accrued%d", k);
		xml.PushAttribute(name, ledger.accrued[k]);
		snprintf(name, sizeof(name), "selection%d", k);
		xml.PushAttribute(name, ledger.selection[k]);
	}
}

void FrontDesk::decodeXML(tinyxml2::XMLElement & xml)
{
	YootFacility::decodeXML(xml);
	for (int k = 0; k < 3; ++k) {
		char name[16];
		snprintf(name, sizeof(name), "accrued%d", k);
		ledger.accrued[k] = std::max(0, xml.IntAttribute(name, 0));
		snprintf(name, sizeof(name), "selection%d", k);
		ledger.selection[k] = std::max(1, std::min(4, xml.IntAttribute(name, 3)));
	}
}

// -------------------------------------------------------------- housekeeping

int Housekeeping::maids() const
{
	if (prototype->yoot && prototype->yoot->belongCapacity > 0) return prototype->yoot->belongCapacity;
	return 6; // Maintenance.t2p TNDF belongCapacity (RMaintenance: 4)
}

int Housekeeping::openMinute() const
{
	if (prototype->yoot && prototype->yoot->closeMinute > 0) return prototype->yoot->openMinute;
	return 420;
}

int Housekeeping::closeMinute() const
{
	if (prototype->yoot && prototype->yoot->closeMinute > 0) return prototype->yoot->closeMinute;
	return 960;
}

// ------------------------------------------------------------------- parking

void ParkingGate::init()
{
	YootFacility::init();
	// The surface entrance band goes on the gate's top row.
	sprite.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-(position.y + size.y - 1) * 36 - interiorLift())});
	setGatePattern(2);
}

void ParkingLot::init()
{
	model = Yoot::ParkingLotModel();
	drivers.clear();
	YootFacility::init();
}

void ParkingLot::decodeXML(tinyxml2::XMLElement & xml)
{
	YootFacility::decodeXML(xml);
	// Parked cars belong to transient drivers; a reloaded lot starts empty.
	model = Yoot::ParkingLotModel();
	drivers.clear();
	setPattern(0);
}

static int driverId(const Person * p)
{
	// Stable for the person's lifetime; never 0 (FUN_1000cdb0 shows id % 13).
	const uintptr_t v = reinterpret_cast<uintptr_t>(p);
	return (int)((v >> 4) & 0x3fffffff) | 1;
}

bool ParkingLot::park(Person * driver)
{
	if (!driver || !model.park(driverId(driver))) return false;
	if (std::find(drivers.begin(), drivers.end(), driver) == drivers.end()) drivers.push_back(driver);
	setPattern(model.pattern);
	return true;
}

bool ParkingLot::leave(Person * driver)
{
	if (!driver || !model.leave(driverId(driver))) return false;
	drivers.erase(std::remove(drivers.begin(), drivers.end(), driver), drivers.end());
	setPattern(model.pattern);
	return true;
}

// ------------------------------------------------------------ fire department

int FireDepartment::firemen() const
{
	if (prototype->yoot && prototype->yoot->belongCapacity > 0) return prototype->yoot->belongCapacity;
	return 8; // FireDepartment.t2p TNDF belongCapacity
}

// ------------------------------------------------------------------ apartment

const char * Apartment::condoPlugin() const
{
	if (prototype->yoot && !prototype->yoot->plugin.empty())
		return prototype->yoot->plugin.c_str();
	return "Apartment.t2p";
}

void Apartment::moveIn()
{
	// Like the office (E4): one inMoney[selection] payment when let.
	const int selection = game->yootPriceSelection(this);
	rentDeposit = prototype->yoot ? prototype->yoot->inMoneyFor(selection) : 500;
	game->yootBook(rentDeposit, "rent_income", "");
}

void Apartment::moveOut()
{
	// A rental: nothing is bought back.
}
