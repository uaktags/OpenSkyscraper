#pragma once
#include "GameObject.h"
#include "Item/Item.h"
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <string>

namespace OT
{
	/** Transient inspector popup.
	 *
	 * Shown when the player inspect-clicks an item while the inspector tool
	 * is active. Renders a small TGUI child window near the top-left of the
	 * viewport with context-aware content: name, position, maintenance cost,
	 * cached evaluation, occupant count, route score, plus an occupant list
	 * (name/state/stress/eval) and - for elevators - cars, floors and
	 * queues. Mirrors the role of InfoDlog/TenantInfo in the Yoot source.
	 *
	 * TODO(Phase 4.1): per-person hit testing (currently we only inspect the
	 * item under the cursor, and list its occupants rather than individual
	 * people). */
	class InspectorDialog : public GameObject
	{
	public:
		InspectorDialog(Game * game);
		~InspectorDialog() { close(); }

		/// Close and detach the popup if currently visible.
		void close();

		/// True if the popup is currently on screen.
		bool isVisible() const { return window != NULL; }

		/// Open the popup for the given item. Closes any prior popup first.
		void showForItem(Item::Item * item);

		/// Re-render the content if visible. Cheap no-op otherwise.
		void refresh();

	private:
		tgui::ChildWindow::Ptr window;
		tgui::Label::Ptr       content;
		tgui::Button::Ptr      floorsButton;  ///< Shown only for elevators.
		Item::Item *           currentItem;

		void buildWindow();
		std::string describeItem(Item::Item * item);
		std::string stateName(Person::State s);
		std::string typeName(Person::Type t);
	};
}
