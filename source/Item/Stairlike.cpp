#include <cassert>
#include "../Game.h"
#include "Stairlike.h"

using namespace OT;
using namespace Item;

const static double kTransitionTime = 1.5;

void Stairlike::init()
{
	Item::init();

	animation = 0;
	frame = 0;
	layer = 1;

	addSprite(&sprite);

	updateSprite();
}

void Stairlike::advance(double dt)
{
	Item::advance(dt);
	double dta = game->time.dta / Time::kBaseSpeed;

	for (People::iterator ip = people.begin(); ip != people.end();)
	{
		Person *p = *(ip++);
		if ((transitionTimes[p] += dta) >= kTransitionTime)
			p->journey.next();
	}

	if (!people.empty())
	{
		animation = fmod(animation + dta * kTransitionTime, 1);
		int newFrame = floor(animation * (frameCount - 1)) + 1;
		if (frame != newFrame)
		{
			frame = newFrame;
			updateSprite();
		}
	}
	else
	{
		animation = 0;
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
