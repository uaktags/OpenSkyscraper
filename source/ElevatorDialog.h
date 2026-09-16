#pragma once
#include "GameObject.h"
#include "Item/Elevator/Elevator.h"
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/ScrollablePanel.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <map>

namespace OT
{
	/** Elevator control panel dialog.
	 *
	 * TGUI child window with per-floor service toggles for an elevator.
	 * The lower schedule panel exposes both weekday and weekend's six
	 * editable periods. Shift-click edits patience, Ctrl-click edits the
	 * threshold, and an ordinary click cycles Full/Express Up/Express
	 * Down. The hidden seventh night period is persisted but not shown.
	 */
	class ElevatorDialog : public GameObject
	{
	public:
		ElevatorDialog(Game * game);
		~ElevatorDialog() { close(); }

		void close();
		bool isVisible() const { return window != NULL; }

		void showForElevator(Item::Elevator::Elevator * elevator);
		void refresh();

	private:
		tgui::ChildWindow::Ptr      window;
		tgui::ScrollablePanel::Ptr  panel;
		tgui::Label::Ptr            header;
		tgui::Button::Ptr           showToggleBtn;
		Item::Elevator::Elevator *  currentElevator;

		/// Button per floor, keyed by floor number.
		std::map<int, tgui::Button::Ptr> floorButtons;
		/// Six editable periods per day, keyed by day * 6 + period.
		std::map<int, tgui::Button::Ptr> scheduleButtons;

		void buildWindow();
		void rebuildFloorButtons();
		void rebuildScheduleButtons();
		void editSchedule(int day, int period);
		std::string buttonLabel(int floor) const;
		std::string scheduleLabel(int day, int period) const;
		std::string showToggleLabel() const;
		void updateShowToggle();
	};
}
