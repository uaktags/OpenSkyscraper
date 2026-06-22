#include "Application.h"
#include "Game.h"
#include "InspectorDialog.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Elevator/Queue.h"
#include "Item/Hotel.h"
#include "Person.h"

#include <cstdio>
#include <sstream>

using namespace OT;

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
	s << item->prototype->name << "\n";
	s << "Floor " << item->position.y
	  << ", x=" << item->position.x << "\n\n";

	if (item->underConstruction)
	{
		double remainAbs = item->constructionEndTime - game->time.absolute;
		if (remainAbs < 0) remainAbs = 0;
		int remainMinutes = static_cast<int>(remainAbs / OT::Time::hourToAbsolute(1.0) * 60.0);
		s << "[under construction]\n"
		  << "Ready in ~" << remainMinutes << " min\n\n";
		return s.str();
	}

	// Core stats common to every tenant.
	s << "Maintenance: " << item->dailyMaintenanceCost() << "/day\n";
	s << "Evaluation:  " << static_cast<int>(item->evaluation) << "/100\n";
	s << "Occupants:   " << item->people.size() << "\n";
	if (!item->lobbyRoute.empty())
		s << "Route score: " << item->lobbyRoute.score() << "\n";
	else
		s << "Route score: (unreachable)\n";

	// Hotel-specific state.
	Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
	if (hotel)
	{
		const char *rs = "?";
		switch (hotel->roomState)
		{
			case Item::Hotel::kClean:    rs = "clean";    break;
			case Item::Hotel::kOccupied: rs = "occupied"; break;
			case Item::Hotel::kDirty:    rs = "dirty";    break;
		}
		s << "Room state:  " << rs << "\n";
		s << "Capacity:    " << hotel->capacity() << "\n";
	}

	// Elevator-specific state.
	Item::Elevator::Elevator * elevator =
		dynamic_cast<Item::Elevator::Elevator *>(item);
	if (elevator)
	{
		s << "\nElevator\n";
		s << "Cars:        " << elevator->cars.size() << "\n";
		s << "Queues:      " << elevator->queues.size() << "\n";
		s << "Serviced fl: " << (elevator->size.y) << "\n";
		s << "Unserviced:  " << elevator->unservicedFloors.size() << "\n";
		// Tally waiting passengers across all queues.
		int waiting = 0;
		for (Item::Elevator::Elevator::Queues::iterator q = elevator->queues.begin();
		     q != elevator->queues.end(); ++q)
			waiting += static_cast<int>((*q)->people.size());
		s << "Waiting:     " << waiting << "\n";
	}

	// Occupant list - up to 8 to keep the popup readable.
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
			  << "  stress " << static_cast<int>(person->stress)
			  << "  eval "  << static_cast<int>(person->eval) << "\n";
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
	window->setTitle(currentItem->prototype->name);
	content->setText(describeItem(currentItem));
	// Show the "Floors..." button only when inspecting an elevator.
	if (floorsButton)
		floorsButton->setVisible(currentItem->isElevator());
}
