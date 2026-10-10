/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include <cstdint>

#include "GameObject.h"
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Label.hpp>

namespace OT
{
	/** Quarterly financial report dialog (Wave U row 49).
	 *
	 * Recreates the report the original SIMTOWER pops right after its
	 * 4:59AM quarterly settlement, drawn on EXE bitmap resource 0x81f4
	 * (installed as "simtower/ui/report" by SimTowerLoader). The
	 * original reconciles the rent tables (resource 0x3E9) against the
	 * maintenance tables (resource 0x3EA); OpenSkyscraper books the same
	 * flows into Money's quarterly category ledger, which this dialog
	 * reads - no separate economy tracking.
	 *
	 * Opened by Game::showReport(): automatically once a quarterly
	 * settlement commits, or on demand with the 'P' key. Rows are
	 * recomputed from Money at open time.
	 *
	 * Original quirk kept: the hotel income row lists whatever the
	 * checkout ledger holds for the quarter even when no guest is in a
	 * hotel room right now (the report reads the ledger, not the live
	 * hotel population).
	 */
	class ReportDialog : public GameObject
	{
	public:
		ReportDialog(Game * game);
		~ReportDialog() { close(); }

		/// Close and detach the dialog if currently visible.
		void close();

		/// Rebuild the window (and repopulate its rows) when visible.
		void reload();

		bool isVisible() const { return window && window->isVisible(); }
		void setVisible(bool visible);

	private:
		tgui::ChildWindow::Ptr window;
		tgui::Label::Ptr       content;
		tgui::Button::Ptr      closeButton;

		/// Decoupled desired-visibility flag, mirroring FinanceWindow, so
		/// reloadGUI() (F1) restores the dialog if it was open.
		bool desiredVisible;

		/// Compose the report text from Money's quarterly accumulators.
		void populate();

		std::string formatMoney(int64_t amount) const;

		void buildWindow();
	};
}
