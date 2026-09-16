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
		showToggleBtn.reset();
		floorButtons.clear();
		scheduleButtons.clear();
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
std::string ElevatorDialog::scheduleLabel(int day, int period) const
{
	if (!currentElevator) return "";
	const Item::Elevator::SchedulePeriod &s = currentElevator->schedules[day][period];
	const char *dayName = day == 0 ? "WD" : "WE";
	const char *mode = s.mode == Item::Elevator::kScheduleExpressUp ? "Up" :
	                   s.mode == Item::Elevator::kScheduleExpressDown ? "Down" : "Full";
	char buf[96];
	snprintf(buf, sizeof(buf), "%s %d: %s  T%d P%d", dayName, period + 1,
	         mode, s.threshold, s.patience);
	return buf;
}

void ElevatorDialog::editSchedule(int day, int period)
{
	if (!currentElevator || day < 0 || day >= Item::Elevator::Elevator::kScheduleDays ||
	    period < 0 || period >= Item::Elevator::Elevator::kSchedulePeriods) return;
	Item::Elevator::SchedulePeriod &s = currentElevator->schedules[day][period];
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) ||
	    sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift))
		s.patience = (s.patience + 1) % 4;
	else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) ||
	         sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl))
		s.threshold = (s.threshold + 1) % 6;
	else
		s.mode = (s.mode + 1) % 3;
	std::map<int, tgui::Button::Ptr>::iterator it = scheduleButtons.find(day * Item::Elevator::Elevator::kSchedulePeriods + period);
	if (it != scheduleButtons.end()) it->second->setText(scheduleLabel(day, period));
}


std::string ElevatorDialog::showToggleLabel() const
{
	if (!currentElevator) return "Show: Yes";
	return currentElevator->showShaft ? "Show: Yes" : "Show: No";
}

void ElevatorDialog::updateShowToggle()
{
	if (showToggleBtn)
		showToggleBtn->setText(showToggleLabel());
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
	const float h = 390 * app->uiScale;
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
	panel->setSize({168 * app->uiScale, 310 * app->uiScale});

	// "Show: Yes/No" toggle — flips the elevator's shaft visibility.
	// When No, the shaft interior is transparent (only rails + cars +
	// motors are drawn) so tenants behind show through.
	showToggleBtn = tgui::Button::create("Show: Yes");
	showToggleBtn->setSize(85 * app->uiScale, 18 * app->uiScale);
	showToggleBtn->setPosition({6 * app->uiScale, 360 * app->uiScale});
	showToggleBtn->onPress([this] {
		if (!currentElevator) return;
		currentElevator->showShaft = !currentElevator->showShaft;
		updateShowToggle();
	});
	window->add(showToggleBtn);

	auto closeBtn = tgui::Button::create("Close");
	closeBtn->setSize(60 * app->uiScale, 18 * app->uiScale);
	closeBtn->setPosition({96 * app->uiScale, 360 * app->uiScale});
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
	rebuildScheduleButtons();
}

void ElevatorDialog::rebuildScheduleButtons()
{
	if (!panel || !currentElevator) return;
	const float btnH = 18 * app->uiScale;
	const float btnW = 160 * app->uiScale;
	float y = (float)floorButtons.size() * (btnH + 2) + 4;
	scheduleButtons.clear();
	for (int day = 0; day < Item::Elevator::Elevator::kScheduleDays; ++day)
		for (int period = 0; period < Item::Elevator::Elevator::kSchedulePeriods; ++period)
		{
			const int key = day * Item::Elevator::Elevator::kSchedulePeriods + period;
			auto btn = tgui::Button::create(scheduleLabel(day, period));
			btn->setSize(btnW, btnH);
			btn->setPosition(0, y);
			btn->setTextSize(9 * app->uiScale);
			btn->onPress([this, day, period] { editSchedule(day, period); });
			panel->add(btn);
			scheduleButtons[key] = btn;
			y += btnH + 2;
		}
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
	updateShowToggle();
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
	for (std::map<int, tgui::Button::Ptr>::iterator it = scheduleButtons.begin();
	     it != scheduleButtons.end(); ++it)
	{
		const int day = it->first / Item::Elevator::Elevator::kSchedulePeriods;
		const int period = it->first % Item::Elevator::Elevator::kSchedulePeriods;
		it->second->setText(scheduleLabel(day, period));
	}
}
