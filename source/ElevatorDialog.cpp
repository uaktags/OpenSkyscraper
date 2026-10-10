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
	              currentElevator->connectsFloor(floor);
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
	renderer->setTitleBarHeight(20 * app->uiScale);
	renderer->setTitleColor(sf::Color(220, 226, 234));
	renderer->setBackgroundColor(sf::Color(24, 28, 36, 245));
	renderer->setTitleBarColor(sf::Color(40, 46, 58));
	renderer->setBorderColor(sf::Color(70, 80, 100));
	renderer->setBorders(1);

	const float w = 210 * app->uiScale;
	const float h = 420 * app->uiScale;
	window->setSize({w, h});
	window->setPosition({static_cast<float>(130 * app->uiScale),
	                     static_cast<float>(40 * app->uiScale)});

	header = tgui::Label::create();
	header->setTextSize(12 * app->uiScale);
	header->setPosition({10 * app->uiScale, 6 * app->uiScale});
	header->getRenderer()->setTextColor(sf::Color(220, 226, 234));
	header->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
	header->setSize({w - 20 * app->uiScale, 44 * app->uiScale});
	window->add(header);

	panel = tgui::ScrollablePanel::create();
	panel->setPosition({10 * app->uiScale, 54 * app->uiScale});
	panel->setSize({w - 20 * app->uiScale, 325 * app->uiScale});
	window->add(panel);

	// "Show: Yes/No" toggle — flips the elevator's shaft visibility.
	showToggleBtn = tgui::Button::create("Show: Yes");
	showToggleBtn->setSize(95 * app->uiScale, 24 * app->uiScale);
	showToggleBtn->setPosition({10 * app->uiScale, 386 * app->uiScale});
	showToggleBtn->setTextSize(11 * app->uiScale);
	auto showR = showToggleBtn->getRenderer();
	showR->setBackgroundColor(sf::Color(45, 52, 65));
	showR->setTextColor(sf::Color::White);
	showR->setBackgroundColorHover(sf::Color(60, 70, 88));
	showR->setBorderColor(sf::Color(75, 88, 110));
	showR->setBorders(1);
	showR->setRoundedBorderRadius(4 * app->uiScale);
	showToggleBtn->onPress([this] {
		if (!currentElevator) return;
		currentElevator->showShaft = !currentElevator->showShaft;
		updateShowToggle();
	});
	window->add(showToggleBtn);

	auto closeBtn = tgui::Button::create("Close");
	closeBtn->setSize(80 * app->uiScale, 24 * app->uiScale);
	closeBtn->setPosition({w - 90 * app->uiScale, 386 * app->uiScale});
	closeBtn->setTextSize(11 * app->uiScale);
	auto closeR = closeBtn->getRenderer();
	closeR->setBackgroundColor(sf::Color(60, 65, 75));
	closeR->setTextColor(sf::Color::White);
	closeR->setBackgroundColorHover(sf::Color(78, 85, 98));
	closeR->setBorderColor(sf::Color(85, 92, 105));
	closeR->setBorders(1);
	closeR->setRoundedBorderRadius(4 * app->uiScale);
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

	const float btnH = 22 * app->uiScale;
	const float btnW = panel->getSize().x - 18 * app->uiScale;
	float y = 0;

	for (int floor = minY; floor < maxY; ++floor)
	{
		bool serviced = currentElevator->connectsFloor(floor);
		auto btn = tgui::Button::create(buttonLabel(floor));
		btn->setSize(btnW, btnH);
		btn->setPosition(0, y);
		btn->setTextSize(11 * app->uiScale);

		auto r = btn->getRenderer();
		r->setRoundedBorderRadius(3 * app->uiScale);
		r->setBorders(1);
		if (serviced) {
			r->setBackgroundColor(sf::Color(32, 64, 102));
			r->setTextColor(sf::Color(230, 240, 255));
			r->setBackgroundColorHover(sf::Color(44, 84, 132));
			r->setTextColorHover(sf::Color::White);
			r->setBorderColor(sf::Color(60, 95, 145));
		} else {
			r->setBackgroundColor(sf::Color(30, 33, 40));
			r->setTextColor(sf::Color(140, 148, 160));
			r->setBackgroundColorHover(sf::Color(42, 46, 56));
			r->setTextColorHover(sf::Color(200, 205, 215));
			r->setBorderColor(sf::Color(50, 56, 68));
		}

		btn->onPress([this, floor] {
			game->toggleElevatorService(currentElevator, floor);
			game->inspectorDialog.refresh();
			refresh();
		});
		panel->add(btn);
		floorButtons[floor] = btn;
		y += btnH + 3;
	}

	panel->setContentSize({btnW, y});
	rebuildScheduleButtons();
}

void ElevatorDialog::rebuildScheduleButtons()
{
	if (!panel || !currentElevator) return;
	const float btnH = 22 * app->uiScale;
	const float btnW = panel->getSize().x - 18 * app->uiScale;
	float y = (float)floorButtons.size() * (btnH + 3) + 6;
	scheduleButtons.clear();
	for (int day = 0; day < Item::Elevator::Elevator::kScheduleDays; ++day)
		for (int period = 0; period < Item::Elevator::Elevator::kSchedulePeriods; ++period)
		{
			const int key = day * Item::Elevator::Elevator::kSchedulePeriods + period;
			auto btn = tgui::Button::create(scheduleLabel(day, period));
			btn->setSize(btnW, btnH);
			btn->setPosition(0, y);
			btn->setTextSize(10 * app->uiScale);

			auto r = btn->getRenderer();
			r->setBackgroundColor(sf::Color(38, 42, 52));
			r->setTextColor(sf::Color(215, 222, 232));
			r->setBackgroundColorHover(sf::Color(52, 58, 72));
			r->setTextColorHover(sf::Color::White);
			r->setBorderColor(sf::Color(62, 70, 86));
			r->setBorders(1);
			r->setRoundedBorderRadius(3 * app->uiScale);

			btn->onPress([this, day, period] { editSchedule(day, period); });
			panel->add(btn);
			scheduleButtons[key] = btn;
			y += btnH + 3;
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
	// Update button labels and styles in case service state changed.
	if (!window || !currentElevator) return;
	for (std::map<int, tgui::Button::Ptr>::iterator it = floorButtons.begin();
	     it != floorButtons.end(); ++it)
	{
		it->second->setText(buttonLabel(it->first));
		bool serviced = currentElevator->connectsFloor(it->first);
		auto r = it->second->getRenderer();
		if (serviced) {
			r->setBackgroundColor(sf::Color(32, 64, 102));
			r->setTextColor(sf::Color(230, 240, 255));
			r->setBackgroundColorHover(sf::Color(44, 84, 132));
			r->setTextColorHover(sf::Color::White);
			r->setBorderColor(sf::Color(60, 95, 145));
		} else {
			r->setBackgroundColor(sf::Color(30, 33, 40));
			r->setTextColor(sf::Color(140, 148, 160));
			r->setBackgroundColorHover(sf::Color(42, 46, 56));
			r->setTextColorHover(sf::Color(200, 205, 215));
			r->setBorderColor(sf::Color(50, 56, 68));
		}
	}
	for (std::map<int, tgui::Button::Ptr>::iterator it = scheduleButtons.begin();
	     it != scheduleButtons.end(); ++it)
	{
		const int day = it->first / Item::Elevator::Elevator::kSchedulePeriods;
		const int period = it->first % Item::Elevator::Elevator::kSchedulePeriods;
		it->second->setText(scheduleLabel(day, period));
	}
}
