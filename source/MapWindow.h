#pragma once
#include "GameObject.h"
#include <SFML/Graphics/RectangleShape.hpp>
#include <TGUI/Backend/Renderer/SFML-Graphics/CanvasSFML.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>

namespace OT
{
	namespace Item { class Item; }

	/** Minimap window.
	 *
	 * TGUI child window containing an SFML canvas that renders a bird's-eye
	 * overview of the tower. Items are drawn as filled rectangles coloured by
	 * the current StatusMode (Normal = by category, Eval = by satisfaction,
	 * Hotel = by cleanliness). Elevators appear as black vertical bars.
	 * Click-to-jump: clicking on the canvas recenters the main viewport on
	 * the corresponding tower coordinate.
	 *
	 * Mirrors MapT.h/c + mapwin.h/c from the Yoot source. Toggle with 'M'.
	 *
	 * TODO(Phase 4.2): the original draws three explicit overlay modes
	 * (Eval/Pric/Hotel) keyed off the StatusMode cycle. We reuse the same
	 * StatusMode tinting as the main viewport for consistency; a dedicated
	 * Pric palette lands when the rent-pricing model does. */
	class MapWindow : public GameObject
	{
	public:
		MapWindow(Game * game);
		~MapWindow() { close(); }

		void close();
		void reload();

		bool isVisible() const { return window && window->isVisible(); }
		void setVisible(bool visible);

		/// Re-render the overview. Called from Game::advance at a throttled
		/// rate; cheap to call when nothing has changed.
		void renderMap();

		/// Convert a click position inside the canvas into tower coordinates
		/// and centre the main viewport there.
		void handleClick(float canvasX, float canvasY);

	private:
		tgui::ChildWindow::Ptr window;
		tgui::CanvasSFML::Ptr  canvas;

		/// Desired visibility. True by default so the map shows on first
		/// load; flipped to false when the user closes the window and back
		/// to true on reopen. Decoupled from the live widget state so that
		/// reload() (called on F1 / UI-scale changes) rebuilds the window
		/// exactly when the player expects to see it.
		bool desiredVisible;

		/// Computed each render pass; used to invert click coordinates.
		int   towerMinX, towerMinY;
		int   towerMaxX, towerMaxY;
		float scale;

		void computeTowerBounds();
		sf::Color colorForItem(Item::Item * item) const;
	};
}
