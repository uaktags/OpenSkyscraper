#pragma once
#include "GameObject.h"
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <map>
#include <string>

namespace OT
{
	namespace Item { class Item; }

	/** Quarterly finance report window.
	 *
	 * TGUI child window summarising the tower's economy for the current
	 * quarter. Mirrors the role of the "Finance" panel in SimTower (the
	 * status-window popout that shows income, maintenance, per-tenant
	 * breakdown, and the running balance).
	 *
	 * The title shows the current Year and Quarter. Below that:
	 *  - Total Income and Total Maintenance for the quarter.
	 *  - A breakdown grouping every tenant/commercial item type present in
	 *    the tower, showing the item Name, Population and Income generated.
	 *  - Net Revenues, Other Income, Construction Costs, Last Quarter
	 *    Balance, and Total Balance.
	 *
	 * Figures are sourced from Money's quarterly accumulators, which are
	 * reset on quarter rollover (see Money::finalizeQuarter).
	 *
	 * Hidden by default; toggle with the 'F' key or the toolbox "Fin"
	 * button. */
	class FinanceWindow : public GameObject
	{
	public:
		FinanceWindow(Game * game);
		~FinanceWindow() { close(); }

		void close();
		void reload();

		bool isVisible() const { return window && window->isVisible(); }
		void setVisible(bool visible);

		/// Recompute all figures and refresh the label text. Called from
		/// Game::advance at a throttled rate.
		void refresh();

	private:
		tgui::ChildWindow::Ptr window;
		tgui::Label::Ptr       content;

		/// Decoupled desired-visibility flag, mirroring MapWindow. False by
		/// default (the finance window is opt-in). Reload honours it so the
		/// window reappears after an F1 rebuild if the player left it open.
		bool desiredVisible;

		/// Per-type income attribution. Returns the quarterly income that
		/// can be attributed to the given prototype id, splitting shared
		/// money categories (e.g. retail_income between Fast Food and
		/// Restaurant) proportionally by population.
		int incomeForType(const std::string & protoId, int population,
		                  const std::map<std::string, int> & sharedPop) const;

		std::string formatMoney(int amount) const;
	};
}
