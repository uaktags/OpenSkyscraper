#include "Application.h"
#include "Game.h"
#include "InspectorDialog.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Elevator/Queue.h"
#include "Item/Hotel.h"
#include "Item/Office.h"
#include "Item/Condo.h"
#include "Item/Shop.h"
#include "Item/PowerRoom.h"
#include "Item/YootVenueItems.h"
#include "Item/YootSpecialItems.h"
#include "Person.h"

#include <cstdio>
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
	transformerButton = NULL;
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
		transformerButton.reset();
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
	content->setSize({208 * app->uiScale, 218 * app->uiScale});
	window->add(content);

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

	// "Transformer" button - YootTower Power Room only (PowerRoom.t2p
	// dialog 0x1bbf "add transformer", FUN_1000c2f0).
	transformerButton = tgui::Button::create("+ Transformer");
	transformerButton->setSize(70 * app->uiScale, 18 * app->uiScale);
	transformerButton->setPosition({144 * app->uiScale, 232 * app->uiScale});
	transformerButton->setTextSize(11 * app->uiScale);
	transformerButton->setVisible(false);
	transformerButton->onPress([this] {
		Item::PowerRoom * room = dynamic_cast<Item::PowerRoom *>(currentItem);
		if (!room) return;
		std::string reason;
		if (!game->eventSystem.power.addTransformer(room, &reason))
			game->timeWindow.showMessage(reason);
		refresh();
	});
	window->add(transformerButton);

	// YootTower venues: the movie (Theater.t2p 0x1000de70), the hall event
	// (EventCenter.t2p FUN_1000d010) or the sponsor (Billboard.t2p
	// FUN_1000bff0) is picked from a list; the subway schedule is expanded
	// with a button (T2Transport::UpGrade).
	venueChoice = tgui::ComboBox::create();
	venueChoice->setSize(208 * app->uiScale, 18 * app->uiScale);
	venueChoice->setPosition({6 * app->uiScale, 210 * app->uiScale});
	venueChoice->setTextSize(11 * app->uiScale);
	venueChoice->setVisible(false);
	venueChoice->onItemSelect([this](const tgui::String &, const tgui::String & id) {
		if (fillingChoice || !currentItem) return;
		std::string reason;
		if (Item::FrontDesk * desk = dynamic_cast<Item::FrontDesk *>(currentItem))
			game->eventSystem.specials.chooseRate(desk, id.toStdString());
		else if (!game->eventSystem.venues.choose(currentItem, id.toStdString(), &reason) && !reason.empty())
			game->timeWindow.showMessage(reason);
		refresh();
	});
	window->add(venueChoice);

	venueButton = tgui::Button::create("Expand schedule");
	venueButton->setSize(100 * app->uiScale, 18 * app->uiScale);
	venueButton->setPosition({6 * app->uiScale, 210 * app->uiScale});
	venueButton->setTextSize(11 * app->uiScale);
	venueButton->setVisible(false);
	venueButton->onPress([this] {
		std::string reason;
		if (!game->eventSystem.venues.upgradeSubway(dynamic_cast<Item::Subway *>(currentItem), &reason) && !reason.empty())
			game->timeWindow.showMessage(reason);
		refresh();
	});
	window->add(venueButton);

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
	s << "Floor " << item->position.y << ", x=" << item->position.x << "\n\n";
	if (item->underConstruction)
	{
		double remainAbs = item->constructionEndTime - game->time.absolute;
		if (remainAbs < 0) remainAbs = 0;
		int remainMinutes = static_cast<int>(remainAbs / OT::Time::hourToAbsolute(1.0) * 60.0);
		s << "[under construction]\nReady in ~" << remainMinutes << " min\n\n";
		return s.str();
	}
	s << "Maintenance: quarterly sweep\n";
	s << "Evaluation:  " << static_cast<int>(item->evaluation) << "/100\n";
	s << "Occupants:   " << item->people.size() << "\n";
	if (!item->lobbyRoute.empty()) s << "Route score: " << item->lobbyRoute.score() << "\n";
	else s << "Route score: (unreachable)\n";

	const std::string &id = item->prototype->id;
	if (id == "office" || id == "condo" || id == "yoot_condo" ||
	    id == "shop" || id == "hotel_single" || id == "hotel_double" ||
	    id == "hotel_suite" || id == "hotel")
	{
		int cls = openSkyDefaultRentClass(id);
		if (id == "shop")
		{
			Item::Shop *shop = dynamic_cast<Item::Shop *>(item);
			if (shop) cls = shop->rentClass;
		}
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

	if (dynamic_cast<Item::PowerRoom *>(item) && game->eventSystem.power.enabled())
	{
		const PowerSystem & power = game->eventSystem.power;
		s << "\nTransformers: " << power.transformers() << "/" << Item::PowerRoom::kMaxTransformers << "\n";
		s << "Supply:      " << power.supply() << "\n";
		s << "Demand:      " << power.demand() << "\n";
		s << "Transformer: $" << power.transformerPriceDollars() << "\n";
		if (power.isBlackOut()) s << "BLACKOUT\n";
	}

	s << game->eventSystem.facilities.describe(item);
	s << game->eventSystem.venues.describe(item);
	s << game->eventSystem.specials.describe(item);

	if (!item->people.empty())
	{
		s << "\nOccupants\n";
		int shown = 0;
		for (Item::Item::People::iterator p = item->people.begin();
		     p != item->people.end() && shown < 8; ++p, ++shown)
		{
			Person *person = *p;
			s << "- " << (person->name.empty() ? typeName(person->type) : person->name)
			  << " [" << stateName(person->state) << "]"
			  << "  banked stress " << static_cast<int>(person->stress) << "/300"
			  << "  eval " << static_cast<int>(person->eval) << "/100\n";
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
	if (transformerButton)
		transformerButton->setVisible(dynamic_cast<Item::PowerRoom *>(currentItem) != NULL &&
		                              game->eventSystem.power.enabled());
	refreshVenueChoice();
}

void InspectorDialog::refreshVenueChoice()
{
	if (!venueChoice || !venueButton) return;
	VenueSystem & venues = game->eventSystem.venues;
	const bool isVenue = venues.enabled() && (dynamic_cast<Item::Theater *>(currentItem) ||
		dynamic_cast<Item::EventCenter *>(currentItem) || dynamic_cast<Item::Billboard *>(currentItem));
	venueButton->setVisible(venues.enabled() && dynamic_cast<Item::Subway *>(currentItem) != NULL);
	if (Item::FrontDesk * desk = venues.enabled() ? dynamic_cast<Item::FrontDesk *>(currentItem) : NULL) {
		// Front Desk room rates (Front.t2p T2DLG 1000), "* " marks the current ones.
		venueChoice->setVisible(true);
		fillingChoice = true;
		venueChoice->removeAllItems();
		for (const auto & c : game->eventSystem.specials.rateChoices(desk))
			venueChoice->addItem(c.first, c.second);
		fillingChoice = false;
		return;
	}
	venueChoice->setVisible(isVenue);
	if (!isVenue) return;
	std::string current;
	if (Item::Theater * t = dynamic_cast<Item::Theater *>(currentItem)) current = t->movie;
	else if (Item::EventCenter * e = dynamic_cast<Item::EventCenter *>(currentItem)) current = e->event;
	else if (Item::Billboard * b = dynamic_cast<Item::Billboard *>(currentItem)) current = b->ad;
	fillingChoice = true;
	venueChoice->removeAllItems();
	for (const auto & c : venues.choices(currentItem))
		venueChoice->addItem(c.first, c.second);
	if (!current.empty()) venueChoice->setSelectedItemById(current);
	fillingChoice = false;
}
