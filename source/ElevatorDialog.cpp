#include "Application.h"
#include "Game.h"
#include "ElevatorDialog.h"
#include "Item/Elevator/Elevator.h"

#include <cstdio>
#include <sstream>

using namespace OT;

ElevatorDialog::ElevatorDialog(Game * game)
	: GameObject(game)
{
	window = NULL;
	panel = NULL;
	header = NULL;
	currentElevator = NULL;
}

void ElevatorDialog::close()
{
	if (window)
	{
		app->gui.remove(window);
		window.reset();
		panel.reset();
		header.reset();
		floorButtons.clear();
	}
	currentElevator = NULL;
}

std::string ElevatorDialog::buttonLabel(int floor) const
{
	bool served = (currentElevator == NULL) ||
	              (currentElevator->unservicedFloors.find(floor) ==
	               currentElevator->unservicedFloors.end());
	char buf[32];
	snprintf(buf, sizeof(buf), "F%d  %s", floor, served ? "[ON]" : "[OFF]");
	return buf;
}

void ElevatorDialog::buildWindow()
{
	window = tgui::ChildWindow::create();
	window->setTitle("Elevator");
	auto renderer = window->getRenderer();
	renderer->setTitleBarHeight(14 * app->uiScale);
	renderer->setBackgroundColor(sf::Color(20, 24, 32, 235));
	renderer->setTitleBarColor(sf::Color(45, 50, 60));
	renderer->setBorderColor(sf::Color(90, 100, 120));
	renderer->setBorders(1);

	const float w = 180 * app->uiScale;
	const float h = 280 * app->uiScale;
	window->setClientSize({w, h});
	window->setPosition({static_cast<float>(130 * app->uiScale),
	                     static_cast<float>(40 * app->uiScale)});

	header = tgui::Label::create();
	header->setTextSize(11 * app->uiScale);
	header->setPosition({6 * app->uiScale, 4 * app->uiScale});
	header->getRenderer()->setTextColor(sf::Color(220, 226, 234));
	header->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
	header->setSize({168 * app->uiScale, 36 * app->uiScale});
	window->add(header);

	panel = tgui::ScrollablePanel::create();
	panel->setPosition({6 * app->uiScale, 44 * app->uiScale});
	panel->setSize({168 * app->uiScale, 210 * app->uiScale});
	window->add(panel);

	auto closeBtn = tgui::Button::create("Close");
	closeBtn->setSize(60 * app->uiScale, 18 * app->uiScale);
	closeBtn->setPosition({60 * app->uiScale, 260 * app->uiScale});
	closeBtn->setTextSize(11 * app->uiScale);
	closeBtn->onPress([this] { close(); });
	window->add(closeBtn);

	window->onClose([this] { close(); });
	app->gui.add(window);
}

void ElevatorDialog::rebuildFloorButtons()
{
	if (!panel || !currentElevator) return;
	panel->removeAllWidgets();
	floorButtons.clear();

	int minY = currentElevator->position.y;
	int maxY = currentElevator->position.y + currentElevator->size.y;

	const float btnH = 18 * app->uiScale;
	const float btnW = 160 * app->uiScale;
	float y = 0;

	for (int floor = minY; floor < maxY; ++floor)
	{
		auto btn = tgui::Button::create(buttonLabel(floor));
		btn->setSize(btnW, btnH);
		btn->setPosition(0, y);
		btn->setTextSize(10 * app->uiScale);
		btn->onPress([this, floor] {
			game->toggleElevatorService(currentElevator, floor);
			// Update just this button's label rather than rebuilding all.
			std::map<int, tgui::Button::Ptr>::iterator it = floorButtons.find(floor);
			if (it != floorButtons.end())
				it->second->setText(buttonLabel(floor));
		});
		panel->add(btn);
		floorButtons[floor] = btn;
		y += btnH + 2;
	}

	// Make the scrollable panel's inner content size match the buttons.
	panel->setContentSize({btnW, y});
}

void ElevatorDialog::showForElevator(Item::Elevator::Elevator * elevator)
{
	if (!elevator) { close(); return; }
	if (!window) buildWindow();
	currentElevator = elevator;

	std::ostringstream s;
	s << elevator->prototype->name << "\n";
	s << "Floors " << elevator->position.y << "-" << (elevator->position.y + elevator->size.y - 1);
	s << " | Cars: " << elevator->cars.size();
	s << "\nCapacity: " << elevator->maxCarCapacity;
	header->setText(s.str());

	rebuildFloorButtons();
}

void ElevatorDialog::refresh()
{
	// Update button labels in case service state changed externally (finger tool).
	if (!window || !currentElevator) return;
	for (std::map<int, tgui::Button::Ptr>::iterator it = floorButtons.begin();
	     it != floorButtons.end(); ++it)
	{
		it->second->setText(buttonLabel(it->first));
	}
}
