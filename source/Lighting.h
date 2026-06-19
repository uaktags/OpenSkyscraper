#pragma once

#include <cstdint>
#include <SFML/Graphics/Color.hpp>
#include "GameObject.h"

namespace OT
{
	/**
	 * Global time-of-day + weather tint that approximates the SimTower
	 * CLUT (color look-up table) shifts performed by AnimeT.h/c in the
	 * original. SFML has no indexed palettes, so the tint is applied
	 * per-sprite in Item::render() / Floor::render() by multiplying each
	 * sprite's existing color with Lighting::compose(). Phase 3.4.
	 *
	 * The tint tracks the Sky's day/dawn/dusk/night/rain state machine
	 * (Sky already publishes from/to/progress), so this class is mostly
	 * stateless - advance() merely refreshes the cached color and fades
	 * the rain intensity in/out smoothly.
	 *
	 * Color anchors (matching the Sky sheet indices):
	 *   0 day       -> white (no change)
	 *   1 dawn/dusk -> warm orange-red
	 *   2 night     -> dark blue, dimmed
	 *   3 cloudy    -> cool gray-blue, slightly dimmed
	 *   4-5 rain    -> like cloudy, a touch darker
	 */
	class Lighting : public GameObject
	{
	public:
		Lighting(Game * game);

		// Refresh the cached tint based on the current Time + Sky state.
		// Called once per frame from Game::advance(), after sky.advance().
		void advance(double dt);

		// The current global tint in [0,255] per channel. Items and other
		// drawables should multiply their per-sprite color by this.
		sf::Color tint() const { return current; }

		// Perceived brightness in [0,1] for inspector/debug use.
		double brightness() const { return currentBrightness; }

		// Compose a sprite's existing color with the current tint. Alpha
		// is preserved (transparency should survive day/night). This is
		// the recommended per-sprite helper used by Item::render().
		sf::Color compose(sf::Color base) const;

		// Channel-wise multiplication of two colors, treating each
		// channel as a [0,255] fraction. Alpha of the first argument is
		// preserved. Declared inline in the header so the unit tests can
		// exercise it without linking the full Lighting.cpp (which
		// depends on Game/Sky/SFML).
		static sf::Color multiply(sf::Color a, sf::Color b)
		{
			return sf::Color(
				static_cast<uint8_t>((static_cast<int>(a.r) * b.r) / 255),
				static_cast<uint8_t>((static_cast<int>(a.g) * b.g) / 255),
				static_cast<uint8_t>((static_cast<int>(a.b) * b.b) / 255),
				a.a
			);
		}

	private:
		sf::Color current;
		double    currentBrightness;

		// Smoothed rain intensity in [0,1]. The Sky flips rainyDay at
		// 05:00 each day, which would otherwise pop the tint; we fade
		// it in across a few seconds of game time.
		double rainIntensity;

		// Look up the anchor color + brightness for a given Sky sheet
		// state index (0=day, 1=dawn/dusk, 2=night, 3=cloudy, 4/5=rain).
		static sf::Color colorForState(int state, double & outBrightness);
	};
}
