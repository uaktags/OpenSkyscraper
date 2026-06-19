/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Phase 3.4 - Weather / Environment CLUT equivalent. */
#include <algorithm>
#include <cmath>

#include "Lighting.h"
#include "Game.h"
#include "Sky.h"
#include "Time.h"

using namespace OT;

Lighting::Lighting(Game * game)
: GameObject(game)
{
	current = sf::Color(255, 255, 255, 255);
	currentBrightness = 1.0;
	rainIntensity = 0.0;
}

sf::Color Lighting::compose(sf::Color base) const
{
	sf::Color out = multiply(base, current);
	out.a = base.a;
	return out;
}

sf::Color Lighting::colorForState(int state, double & outBrightness)
{
	switch (state)
	{
		// Day - white, no change.
		case 0:
			outBrightness = 1.00;
			return sf::Color(255, 255, 255);
		// Dawn / dusk - warm orange-red sunrise/sunset tint.
		case 1:
			outBrightness = 0.96;
			return sf::Color(255, 196, 140);
		// Night - dark blue, moderately dimmed.
		case 2:
			outBrightness = 0.55;
			return sf::Color(150, 168, 220);
		// Cloudy - cool gray-blue, slightly dimmed.
		case 3:
			outBrightness = 0.82;
			return sf::Color(180, 198, 220);
		// Rain animation frames - like cloudy but a touch darker.
		case 4:
		case 5:
			outBrightness = 0.78;
			return sf::Color(170, 192, 222);
		default:
			outBrightness = 1.00;
			return sf::Color(255, 255, 255);
	}
}

void Lighting::advance(double dt)
{
	// Smoothly fade the rain intensity toward whatever the Sky currently
	// says. Sky sets rainyDay at 05:00 and rain visuals run 07:00-17:00,
	// so we mirror that window. Fade rate is in absolute-time units so
	// it stays consistent across game speeds.
	const Sky & sky = game->sky;
	const bool raining = sky.rainyDay && game->time.hour >= 7.0 && game->time.hour < 17.0;
	const double target = raining ? 1.0 : 0.0;
	const double dta = game->time.dta / Time::kBaseSpeed;
	const double fade = std::min(1.0, dta * 5.0);
	rainIntensity += (target - rainIntensity) * fade;
	if (rainIntensity < 1e-3) rainIntensity = 0.0;
	if (rainIntensity > 0.999) rainIntensity = 1.0;

	// Build the base tint by interpolating the Sky's from->to colors.
	double bFrom = 1.0, bTo = 1.0;
	const sf::Color cFrom = colorForState(sky.from, bFrom);
	const sf::Color cTo   = colorForState(sky.to,   bTo);
	const double p = std::max(0.0, std::min(1.0, sky.progress));

	const double r = cFrom.r * (1.0 - p) + cTo.r * p;
	const double g = cFrom.g * (1.0 - p) + cTo.g * p;
	const double b = cFrom.b * (1.0 - p) + cTo.b * p;
	currentBrightness   = bFrom * (1.0 - p) + bTo * p;

	// Blend in a rain dimming factor on top: rain pulls the tint a bit
	// darker and bluer. Weighted by rainIntensity so the change fades in
	// rather than popping at 07:00 / 17:00.
	const double rainDim = 0.85 + 0.15 * (1.0 - rainIntensity); // 1.0 -> 0.85
	const double rainBlueShift = rainIntensity; // 0 -> 1

	double rr = r * rainDim;
	double gg = g * rainDim;
	double bb = b * rainDim;

	// Push slight extra blue during rain.
	bb = bb + (220.0 - bb) * 0.10 * rainBlueShift;

	current.r = static_cast<uint8_t>(std::max(0.0, std::min(255.0, rr)));
	current.g = static_cast<uint8_t>(std::max(0.0, std::min(255.0, gg)));
	current.b = static_cast<uint8_t>(std::max(0.0, std::min(255.0, bb)));
	current.a = 255;
}
