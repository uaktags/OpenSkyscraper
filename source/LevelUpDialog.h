#pragma once

#include "GameObject.h"
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include <vector>

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

		/// Per frame: steps the YootTower grade-up animation.
		void advance(double dt);

	private:
		tgui::ChildWindow::Ptr window;
		tgui::Label::Ptr       heading;
		tgui::Label::Ptr       unlocks;
		tgui::Button::Ptr      okButton;

		/// YootTower: the world's grade-up animation (T2.exe FUN_0041ca80,
		/// dialog 9101 item 0x68: T2WorldDef::SetupGradeUpAnimation
		/// 0x10072e70 -> T2DlgItemAnimation): the bitmap of the WORLDDEF
		/// grade row, 144x96 frames stacked vertically, one every 100 ms,
		/// looping back to the row's frame (OnT2Timer 0x10022470).
		tgui::Picture::Ptr      anim;
		std::vector<tgui::Texture> animFrames;
		int animFrame = 0, animLoop = 0;
		double animClock = 0;

		/// Build the list of prototype names that require exactly the
		/// given rating (i.e. freshly unlocked by this promotion).
		std::string unlockedAt(int rating) const;

		void buildWindow();
	};
}
