#include "Application.h"
#include "Game.h"
#include "InspectorDialog.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Elevator/Queue.h"
#include "Item/Hotel.h"
#include "Item/Office.h"
#include "Item/Condo.h"
#include "Item/Shop.h"
#include "Item/Cinema.h"
#include "Item/PartyHall.h"
#include "Item/Metro.h"
#include "Item/Parking.h"
#include "NameManager.h"
#include "Person.h"
#include "TenantRules.h"

#include <cstdio>
#include <cmath>
#include <sstream>

using namespace OT;
extern int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass);
extern int openSkyDefaultRentClass(const std::string &prototypeId);

InspectorDialog::InspectorDialog(Game * game)
	: GameObject(game)
{
	window = NULL;
	content = NULL;
	floorsButton = NULL;
	currentItem = NULL;
}

void InspectorDialog::close()
{
	if (window)
	{
		app->gui.remove(window);
		window.reset();
		content.reset();
		floorsButton.reset();
		for (int i = 0; i < 4; ++i) classButtons[i].reset();
	}
	currentItem = NULL;
	// Clear the debug route overlay so the green line doesn't linger
	// after the inspector is dismissed.
	game->visualizeRoute.clear();
}

void InspectorDialog::buildWindow()
{
	window = tgui::ChildWindow::create();
	window->setTitle("Inspector");
	auto renderer = window->getRenderer();
	renderer->setTitleBarHeight(14 * app->uiScale);
	renderer->setBackgroundColor(sf::Color(20, 24, 32, 230));
	renderer->setTitleBarColor(sf::Color(45, 50, 60));
	renderer->setBorderColor(sf::Color(90, 100, 120));
	renderer->setBorders(1);

	window->setClientSize({220 * app->uiScale, 260 * app->uiScale});

	// Park the dialog near the top-left of the viewport so it doesn't
	// overlap the toolbox on the left or the time window at the top.
	window->setPosition({static_cast<float>(120 * app->uiScale),
	                     static_cast<float>(40 * app->uiScale)});

	content = tgui::Label::create();
	content->setTextSize(11 * app->uiScale);
	content->setPosition({6 * app->uiScale, 4 * app->uiScale});
	content->getRenderer()->setTextColor(sf::Color(220, 226, 234));
	content->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Automatic);
	content->setSize({208 * app->uiScale, 198 * app->uiScale});
	window->add(content);

	// Rent class selector (T12): four buttons, most expensive class first,
	// each labelled with its quarterly rent. Shown for office, condo, shop
	// and hotel items.
	for (int i = 0; i < 4; ++i)
	{
		classButtons[i] = tgui::Button::create("");
		classButtons[i]->setSize(50 * app->uiScale, 18 * app->uiScale);
		classButtons[i]->setPosition({(6 + i * 53) * app->uiScale, 208 * app->uiScale});
		classButtons[i]->setTextSize(10 * app->uiScale);
		classButtons[i]->setVisible(false);
		classButtons[i]->onPress([this, i] {
			if (!currentItem) return;
			currentItem->setRentClass(i);
			refresh();
		});
		window->add(classButtons[i]);
	}

	// "Floors..." button - only visible when inspecting an elevator. Opens
	// the ElevatorDialog with per-floor service toggles.
	floorsButton = tgui::Button::create("Floors...");
	floorsButton->setSize(60 * app->uiScale, 18 * app->uiScale);
	floorsButton->setPosition({6 * app->uiScale, 232 * app->uiScale});
	floorsButton->setTextSize(11 * app->uiScale);
	floorsButton->setVisible(false);
	floorsButton->onPress([this] {
		if (!currentItem) return;
		Item::Elevator::Elevator * e =
			dynamic_cast<Item::Elevator::Elevator *>(currentItem);
		if (e) game->elevatorDialog.showForElevator(e);
	});
	window->add(floorsButton);

	// Close button at the bottom.
	auto closeBtn = tgui::Button::create("Close");
	closeBtn->setSize(60 * app->uiScale, 18 * app->uiScale);
	closeBtn->setPosition({78 * app->uiScale, 232 * app->uiScale});
	closeBtn->setTextSize(11 * app->uiScale);
	closeBtn->onPress([this] { close(); });
	window->add(closeBtn);

	// Tear down when the user clicks the title-bar close widget.
	window->onClose([this] { close(); });

	app->gui.add(window);
}

std::string InspectorDialog::stateName(Person::State s)
{
	switch (s)
	{
		case Person::kWandering:  return "wandering";
		case Person::kHome:       return "home";
		case Person::kCommuting:  return "commuting";
		case Person::kWorking:    return "working";
		case Person::kLunch:      return "at lunch";
		case Person::kShopping:   return "shopping";
		case Person::kReturning:  return "returning";
		case Person::kResting:    return "resting";
		case Person::kIdle:       return "idle";
	}
	return "?";
}

std::string InspectorDialog::typeName(Person::Type t)
{
	switch (t)
	{
		case Person::kMan:              return "Man";
		case Person::kSalesman:         return "Salesman";
		case Person::kWoman1:           return "Woman";
		case Person::kChild:            return "Child";
		case Person::kWoman2:           return "Woman";
		case Person::kHousekeeper:      return "Housekeeper";
		case Person::kWomanWithChild1:  return "Woman w/ child";
		case Person::kWomanWithChild2:  return "Woman w/ child";
		case Person::kSecurity:         return "Security";
	}
	return "?";
}

std::string InspectorDialog::describeItem(Item::Item * item)
{
	std::ostringstream s;
	s << (item->displayName.empty() ? item->prototype->name : item->displayName) << "\n";
	s << "Floor " << item->position.y << ", x=" << item->position.x
	  << "  Size " << item->size.x << "x" << item->size.y << "\n\n";
	if (item->underConstruction)
	{
		double remainAbs = item->constructionEndTime - game->time.absolute;
		if (remainAbs < 0) remainAbs = 0;
		int remainMinutes = static_cast<int>(remainAbs / OT::Time::hourToAbsolute(1.0) * 60.0);
		s << "[under construction]\nReady in ~" << remainMinutes << " min\n\n";
		if (item->prototype->id == "restaurant" || item->prototype->id == "fastfood")
			s << "Opens tomorrow\n";
		return s.str();
	}
	s << "Maintenance: quarterly sweep\n";
	s << "Evaluation:  " << static_cast<int>(item->evaluation) << "/100\n";
	s << "Occupants:   " << item->people.size() << "\n";
	if (!item->lobbyRoute.empty()) s << "Route score: " << item->lobbyRoute.score() << "\n";
	else s << "Route score: (unreachable)\n";

	const std::string &id = item->prototype->id;
	if (item->hasTenantRecord() && id != "restaurant" && id != "fastfood")
	{
		const int cls = item->tenant.rentClass;
		s << "Rent class:    " << cls << "\n";
		s << "Rent/quarter: $" << openSkyQuarterlyRent(id, cls) << "\n";
	}

	Item::Hotel *hotel = dynamic_cast<Item::Hotel *>(item);
	if (hotel)
	{
		const char *rs = "?";
		switch (hotel->roomState)
		{
			case Item::Hotel::kClean:    rs = "clean";    break;
			case Item::Hotel::kOccupied: rs = "occupied"; break;
			case Item::Hotel::kDirty:    rs = "dirty";    break;
			case Item::Hotel::kInfested: rs = "infested"; break;
		}
		s << "Room state:  " << rs << "\n";
		s << "Capacity:    " << hotel->capacity() << "\n";
	}

	Item::Elevator::Elevator *elevator =
		dynamic_cast<Item::Elevator::Elevator *>(item);
	if (elevator)
	{
		s << "\nElevator\n";
		s << "Cars:        " << elevator->cars.size() << "\n";
		s << "Queues:      " << elevator->queues.size() << "\n";
		s << "Serviced fl: " << elevator->size.y << "\n";
		s << "Unserviced:  " << elevator->unservicedFloors.size() << "\n";
		int waiting = 0;
		for (Item::Elevator::Elevator::Queues::iterator q = elevator->queues.begin();
		     q != elevator->queues.end(); ++q)
			waiting += static_cast<int>((*q)->people.size());
		s << "Waiting:     " << waiting << "\n";
	}

	{
		const std::vector<std::string> comments = tenantComments(item);
		if (!comments.empty())
		{
			s << "\n";
			for (size_t i = 0; i < comments.size(); ++i) s << comments[i] << "\n";
		}
	}

	if (!item->people.empty())
	{
		s << "\nOccupants\n";
		int shown = 0;
		for (Item::Item::People::iterator p = item->people.begin();
		     p != item->people.end() && shown < 8; ++p, ++shown)
		{
			Person *person = *p;
			s << "- " << personLabel(person)
			  << " [" << stateName(person->state) << "]"
			  << "  " << personWhereabouts(person)
			  << "  wait " << person->bankedMean() << "\n";
		}
		if (item->people.size() > 8)
			s << "...+" << (item->people.size() - 8) << " more\n";
	}
	return s.str();
}

void InspectorDialog::showForItem(Item::Item * item)
{
	if (!item) { close(); return; }
	if (!window) buildWindow();
	currentItem = item;
	refresh();
}

void InspectorDialog::refresh()
{
	if (!window || !content || !currentItem) return;
	window->setTitle(currentItem->displayName.empty() ? currentItem->prototype->name : currentItem->displayName);
	content->setText(describeItem(currentItem));
	// Show the "Floors..." button only when inspecting an elevator.
	if (floorsButton)
		floorsButton->setVisible(currentItem->isElevator());
	refreshClassButtons();
}

void InspectorDialog::refreshClassButtons()
{
	if (!currentItem) return;
	const std::string & id = currentItem->prototype->id;
	const bool selectable = currentItem->hasTenantRecord() && id != "restaurant" &&
	                        id != "fastfood" && !currentItem->underConstruction;
	for (int i = 0; i < 4; ++i)
	{
		if (!classButtons[i]) continue;
		classButtons[i]->setVisible(selectable);
		if (!selectable) continue;
		const int rent = openSkyQuarterlyRent(id, i);
		char label[32];
		if (rent >= 1000) snprintf(label, sizeof(label), "$%dk", rent / 1000);
		else snprintf(label, sizeof(label), "$%d", rent);
		std::string text = label;
		if (currentItem->tenant.rentClass == i) text = "[" + text + "]";
		classButtons[i]->setText(text);
	}
}

std::string InspectorDialog::personLabel(Person * person)
{
	using NM = NameManager;
	NM::Role role = NM::Role::Visitor;
	const void * key = person->home;
	if (person->home)
	{
		const std::string & hid = person->home->prototype->id;
		if (hid == "office") role = NM::Role::Office;
		else if (hid == "condo") role = NM::Role::Condo;
		else role = NM::Role::HotelGuest;
	}
	else if (person->type == Person::kHousekeeper || person->type == Person::kSecurity)
		role = NM::Role::Staff;
	std::string custom = key ? NM::customName(key, person->member) : std::string();
	const bool vip = person->name == "VIP";
	if (custom.empty() && !person->name.empty() && !vip) custom = person->name;
	return NM::personLabel(custom, vip, role, person->member, (int)person->type, game->time.hour);
}

std::string InspectorDialog::personWhereabouts(Person * person)
{
	std::string homeType = person->home ? person->home->prototype->name : std::string("Tower");
	std::string floorText = "Floor " + std::to_string(person->home ? person->home->position.y :
		person->at ? person->at->position.y : 0);
	return NameManager::whereabouts(person->goingTo, homeType, floorText);
}

std::vector<std::string> InspectorDialog::tenantComments(Item::Item * item)
{
	std::vector<std::string> out;
	const std::string & id = item->prototype->id;
	const bool isHotel = id == "hotel_single" || id == "hotel_double" ||
	                     id == "hotel_suite" || id == "hotel";
	const bool isFood = id == "restaurant" || id == "fastfood";

	// 1. Occupied stressed office / condo.
	if ((id == "office" || id == "condo") && item->isOccupied() &&
	    item->tenant.category == TenantRules::kStressed)
		out.push_back("Conditions are terrible");

	// 2. Infested room.
	if (Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item))
		if (hotel->roomState == Item::Hotel::kInfested) out.push_back("Room is too dirty");

	// 3. Transport.
	if (id == "office" || id == "condo" || id == "shop" || isFood || isHotel ||
	    id == "cinema" || id == "partyhall")
	{
		if (item->lobbyRouteComputed && item->lobbyRoute.empty() && item->position.y != 0)
			out.push_back("No transportation connected");
		else if (!item->lobbyRoute.empty())
		{
			const Item::Item * transport = nullptr;
			const double originX = item->position.x + item->size.x / 2.0;
			for (size_t i = 0; i < item->lobbyRoute.nodes.size(); ++i)
			{
				const Item::Item * n = item->lobbyRoute.nodes[i].item;
				if (n && (n->isStairlike() || n->isElevator())) transport = n;
			}
			if (transport)
			{
				const int tier = TenantRules::distanceTier(
					(int)std::abs(transport->position.x - originX));
				if (tier > 0)
				{
					const char * name = transport->isElevator() ? "Elevator" :
						transport->prototype->id == "escalator" ? "Escalator" : "Stairs";
					out.push_back(std::string(name) + (tier == 2 ? " is very far away" : " is far away"));
				}
			}
		}
	}

	// 4. Cinema.
	if (Item::Cinema * cinema = dynamic_cast<Item::Cinema *>(item))
		out.push_back(TenantRules::cinemaLine(cinema->movieAgeDays, cinema->quotaMatinee));

	// 5. Shop.
	if (Item::Shop * shop = dynamic_cast<Item::Shop *>(item))
	{
		if (!shop->let) out.push_back("No renters");
		else
		{
			out.push_back(TenantRules::businessLine(shop->customersToday));
			if (game->time.isHoliday() && shop->customersToday >= 35)
				out.push_back("Weekends attract more customers");
			else if (game->sky.rainyDay && shop->customersToday < 35)
				out.push_back("Rain might cause fewer customers");
		}
	}

	// 6. Medical center out of reach.
	if ((id == "office" || id == "condo") && game->rating >= 2)
	{
		const int z = TenantRules::lookupZone(item->position.y);
		const bool own = z <= 6 && !game->venueLists[z][3].empty();
		if (!own && game->venueLists[0][3].empty())
			out.push_back("Medical Center is too far away");
	}

	// 7. Parking.
	if (Item::Parking * parking = dynamic_cast<Item::Parking *>(item))
	{
		bool unusable = false;
		const std::vector<Item::Parking::Bay> & bays = parking->bays();
		for (size_t i = 0; i < bays.size(); ++i) if (!bays[i].usable) unusable = true;
		if (unusable) out.push_back("Not connected to Ramp");
		for (size_t i = 0; i < bays.size(); ++i)
			if (bays[i].occupied && bays[i].person)
				out.push_back("Car for " + personLabel(bays[i].person));
	}

	// 8. Dead retail zone.
	if ((id == "shop" || isFood) && TenantRules::registrationZone(item->position.y) < 0)
		out.push_back("Too far from Lobby or Skylobby");

	// 9. Food under construction.
	if (isFood && item->underConstruction) out.push_back("Opens tomorrow");

	// 10. Party.
	if (id == "partyhall" && !item->people.empty()) out.push_back("A Party is happening!");

	// 11. Recycling.
	if (id == "recycling" && !game->recyclingAdequate) out.push_back("Full of Garbage");

	// 12. Metro.
	if (id == "metro")
	{
		const double h = game->time.hour;
		if (h < 10.0) out.push_back("First train is not coming");
		else if (h >= 21.0) out.push_back("Last train is gone");
		else if (!item->people.empty()) out.push_back("Crowded with passengers");
	}

	// 13. Noisy neighbour.
	if (Item::Item * source = game->judgeSystem.noiseSource(item))
		out.push_back(source->prototype->name + " neighbor is noisy");
	return out;
}
