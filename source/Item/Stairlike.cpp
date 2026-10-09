#include <cassert>
#include <algorithm>
#include <cmath>
#include "../Game.h"
#include "Stairlike.h"

using namespace OT;
using namespace Item;

const static double kTransitionTime = 20;

void Stairlike::init()
{
	Item::init();

frame = 0;
layer = 1;

	addSprite(&sprite);

	updateSprite();
}

void Stairlike::advance(double dt)
{
	Item::advance(dt);
	double dta = game->time.dta * Time::kSimFramesPerDay;
	// Animation pacing ported from OpenSkyWeb stairlike.js: frames follow
	// the actual walking progress of the person on the stairs, with
	// direction-aware frames on straight stairs (1..6 up, 7..13 down),
	// instead of a free-running full-speed loop.
	double activeProgress = 0.0;
	int activeDirection = 1; // 1 = up, -1 = down
	bool hasPeople = false;
	for (People::iterator ip = people.begin(); ip != people.end();)
	{
		Person *p = *(ip++);
		const double t = (transitionTimes[p] += dta);
		if (t >= kTransitionTime) {
			p->journey.next();
		} else {
			hasPeople = true;
			const double progress = std::max(0.0, std::min(1.0, t / kTransitionTime));
			if (progress >= activeProgress) {
				activeProgress = progress;
				const int toFloor = p->journey.toFloor;
				activeDirection = (toFloor >= position.y + std::max(1, size.y - 1)) ? 1 : -1;
			}
		}
	}
	if (hasPeople)
	{
		int newFrame = 1;
		if (frameCount == 14) {
			// Straight stairs: frame 0 empty, frames 1..6 UP, 7..13 DOWN.
			if (activeDirection >= 0) {
				const int step = std::min(5, (int)std::floor(activeProgress * 6));
				newFrame = 1 + step;
			} else {
				const int step = std::min(6, (int)std::floor(activeProgress * 7));
				newFrame = 7 + step;
			}
		} else {
			// Escalator (8 frames) or spiral (11): 0 empty, 1..N-1 moving,
			// tracking the walker's progress.
			const int movingFrames = frameCount - 1;
			const int step = std::min(movingFrames - 1, (int)std::floor(activeProgress * movingFrames));
			newFrame = 1 + step;
		}
		if (frame != newFrame)
		{
			frame = newFrame;
			updateSprite();
		}
	}
	else
	{
		if (frame != 0)
		{
			frame = 0;
			updateSprite();
		}
	}
}
void Stairlike::updateSprite()
{
	int w = sprite.getTexture().getSize().x / frameCount;
	int h = sprite.getTexture().getSize().y;

	sprite.setOrigin({0.f, (float)h});
	sprite.setTextureRect(sf::IntRect({w * frame, 0}, {w, h}));
	sprite.setPosition({(float)getPosition().x * 8, (float)-getPosition().y * 36});
}

bool Stairlike::connectsFloor(int floor) const
{
	return position.y == floor || position.y + size.y - 1 == floor;
}

bool Stairlike::containsPoint(const double2 & pt)
{
	// Use the sprite's actual global bounds rather than the full tile
	// footprint. Stairs and escalators occupy a narrow strip at the left
	// edge of their logical size.x-wide footprint; the rest should pass
	// clicks through to the tenant behind.
	sf::FloatRect b = sprite.getGlobalBounds();
	// Convert screen-space bounds (Y down) to tower-space (Y up).
	rectd r(b.position.x, -b.position.y - b.size.y, b.size.x, b.size.y);
	return r.containsPoint(pt);
}

void Stairlike::addPerson(Person *p)
{
	transitionTimes[p] = 0;
	Item::addPerson(p);
}

void Stairlike::removePerson(Person *p)
{
	transitionTimes.erase(p);
	Item::removePerson(p);
}
