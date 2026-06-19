#pragma once

#include "GameObject.h"
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Button.hpp>

namespace OT
{
	/** Modal shown on star-rating promotion (Phase 3.2).
	 *
	 * Replaces the transient timeWindow.showMessage("Promoted to N
	 * stars!") so the player gets a more noticeable acknowledgement of
	 * the promotion, including the list of items that just unlocked.
	 * Mirrors the role of the original SimTower level-up dialog.
	 *
	 * Lifecycle: ratingMayIncrease() calls showForRating(newRating)
	 * whenever it bumps the rating. The dialog auto-closes on Close
	 * button, title-bar X, or Escape (handled by ChildWindow). It does
	 * not pause the simulation - players that dismiss it quickly just
	 * see the rest of the UI update underneath.
	 */
	class LevelUpDialog : public GameObject
	{
	public:
		LevelUpDialog(Game * game);
		~LevelUpDialog() { close(); }

		/// Close and detach the popup if currently visible.
		void close();

		/// True if the popup is currently on screen.
		bool isVisible() const { return window != nullptr; }

		/// Open the popup celebrating the given newly-attained rating
		/// (0-based: 0 = just reached 1 star, 4 = just reached 5 stars).
		/// Closes any prior popup first.
		void showForRating(int newRating);

	private:
		tgui::ChildWindow::Ptr window;
		tgui::Label::Ptr       heading;
		tgui::Label::Ptr       unlocks;
		tgui::Button::Ptr      okButton;

		/// Build the list of prototype names that require exactly the
		/// given rating (i.e. freshly unlocked by this promotion).
		std::string unlockedAt(int rating) const;

		void buildWindow();
	};
}
