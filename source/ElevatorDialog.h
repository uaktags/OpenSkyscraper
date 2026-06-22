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
	 * Each floor in the elevator's span gets a button showing its number
	 * and current service state; clicking flips the elevator's
	 * unservicedFloors set via Game::toggleElevatorService().
	 *
	 * Mirrors ElvDlogT.h/c from the Yoot source. Currently opened from the
	 * InspectorDialog when inspecting an elevator.
	 *
	 * TODO(Phase 5.3): WD/WE service modes, express-to-top/bottom buttons,
	 * time-before-departing slider. */
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

		void buildWindow();
		void rebuildFloorButtons();
		std::string buttonLabel(int floor) const;
		std::string showToggleLabel() const;
		void updateShowToggle();
	};
}
