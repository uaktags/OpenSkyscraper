#include <algorithm>
#include <cmath>
#include "../../Application.h"
#include "../../Game.h"
#include "../../Person.h"
#include "../../Sprite.h"
#include "Elevator.h"
#include "Queue.h"
#include "../../YootPeopleArt.h"
#include "../../YootRules.h"
#include <vector>

using namespace OT;
using namespace OT::Item::Elevator;

/// Wait stress is kept in the person's journey bank and delivered when the
/// journey reaches its destination. Service queues do not bank wait stress.

Queue::Queue(Elevator *e)
	: GameObject(e->game),
	  elevator(e),
	  floor(0),
	  direction(Elevator::kNone),
	  width(400),
	  called(false),
	  callTime(0),
	  answered(false),
	  steppingInside(false),
	  fullRetryFrame(0),
	  waitStartFrame(0)
{
}

Queue::~Queue()
{
	for (People::iterator ip = people.begin(); ip != people.end();)
	{
		LOG(DEBUG, "forcing passenger %p ahead", (*ip));
		(*(ip++))->journey.next();
	}
}

/// Returns the absolute time that has passed since the elevator was called from this queue.
double Queue::getWaitDuration()
{
	return game->time.absolute - callTime;
}

void Queue::addPerson(Person *p)
{
	if (!p) return;
	if ((int)people.size() >= kQueueCapacity)
	{
		// TripT rejects the 41st rider and applies a single +5 penalty per
		// 16-frame full-queue retry. Service staff are exempt from wait stress.
		const double frame = game->time.absolute / Time::kBaseSpeed;
		if (!elevator->isService() && frame >= fullRetryFrame)
		{
			p->addJourneyStress((double)kQueueFullPenalty);
			fullRetryFrame = frame + kQueueFullRetryFrames;
		}
		return;
	}
	people.push_back(p);
	if (!called) callElevator();
}

void Queue::removePerson(Person *p)
{
	people.remove(p);
}

Person *Queue::popPerson()
{
	if (people.empty())
		return NULL;
	Person *p = people.front();
	people.pop_front();
	return p;
}

void Queue::callElevator()
{
	if (!called && !people.empty())
	{
		called = true;
		answered = false;
		callTime = game->time.absolute;
		waitStartFrame = game->time.absolute / Time::kBaseSpeed;
		elevator->called(this);
	}
}

void Queue::applyWatchdog()
{
	if (elevator->isService() || people.empty()) return;
	const double frame = game->time.absolute / Time::kBaseSpeed;
	if (frame - waitStartFrame < kQueueWatchdogFrames) return;
	for (People::iterator it = people.begin(); it != people.end(); ++it)
		(*it)->addJourneyStress(300.0); // failed trip banks the wait cap
	called = false;
	answered = false;
	waitStartFrame = frame + kQueueFullRetryFrames;
	fullRetryFrame = frame + kQueueFullRetryFrames;
}

void Queue::advance(double dt)
{
	if (!people.empty() && !called)
	{
		const double frame = game->time.absolute / Time::kBaseSpeed;
		// Failed calls are retried at the original one-in-sixteen cadence,
		// rather than continuously hammering dispatch.
		if (frame >= waitStartFrame && ((int)frame % kQueueFullRetryFrames) == 0)
			callElevator();
	}
	if (!people.empty() && people.front()->yootRules())
	{
		// YootTower (P1, T2People::IdleWaitMover 0x10048880): every waiting
		// person idles once per 2 game minutes and gains +5 stress; at 300
		// they give up and jump to their destination. Replaces the
		// SimTower watchdog and journey-bank wait stress.
		const long long tick = Yoot::absoluteMinute(game->time.absolute) / Yoot::kPeopleIdleMinutes;
		if (yootIdleTick < 0) yootIdleTick = tick;
		long long ticks = tick - yootIdleTick;
		yootIdleTick = tick;
		for (; ticks > 0 && !people.empty(); --ticks)
		{
			std::vector<Person *> waiting(people.begin(), people.end());
			for (size_t i = 0; i < waiting.size(); ++i)
				waiting[i]->yootWaitTick(); // may remove the person from this queue
		}
		return;
	}
	yootIdleTick = -1;
	applyWatchdog();
	if (elevator->isService()) return;
	const double frameDelta = game->time.dta / Time::kBaseSpeed;
	for (People::iterator it = people.begin(); it != people.end(); ++it)
	{
		const double forgiveness = (game->mainLobby && floor == game->mainLobby->position.y)
			? (game->mainLobby->size.y >= 3 ? 50.0 : 25.0) : 0.0;
		const double scale = std::max(0.0, frameDelta - forgiveness / 300.0 * frameDelta);
		(*it)->addJourneyStress(scale);
	}
}

void Queue::draw(sf::RenderTarget &target, sf::RenderStates states) const
{
	render(target);
}
void Queue::render(sf::RenderTarget &target) const
{
	// Start the queue 16 pixels away from the elevator.
	int x = 16;

	Sprite s;
	s.setTexture(app->bitmaps["simtower/elevator/people"]);
	s.setOrigin({0.f, 24.f});
	s.setPosition({0.f, 0.f});

	sf::Color statusTint = sf::Color::White;
	if (game->statusMode == Game::kHotel)
	{
		statusTint = sf::Color(110, 110, 110, 160);
	}

	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255) || statusTint != sf::Color::White);

	for (People::const_iterator ip = people.begin(); ip != people.end(); ip++)
	{
		Person *p = *ip;

		// If people are stepping into an elevator car, we have to draw the frontmost person
		// differently.
		bool stepping = (steppingInside && people.front() == p);

		// If the person is stepping inside we may move the x coordinate back by 16 pixels. This is
		// because the queue is formed starting 16 pixels away from the elevator. The stepping ani-
		// mations are designed to be drawn touching the elevator which would be 0 pixels. And since
		// we start at x = 16 we may move it back by 16 to start at 0.
		if (stepping)
			x -= 16;

		// YootTower: the world silhouette, facing the elevator (rows 0/3;
		// the walking frame while stepping in).
		const YootSilhouetteFrame yoot = yootPersonSilhouette(game, p);
		if (yoot.texture)
		{
			Sprite y;
			y.setTexture(*yoot.texture);
			y.setTextureRect(yootSilhouetteRect((direction == Elevator::kUp ? 0 : 3) + (stepping ? 1 : 0)));
			y.setOrigin({0.f, 36.f});
			y.setPosition({(direction == Elevator::kUp ? -x - 8.f : elevator->size.x * 8.f + x - 8.f) + elevator->getPositionPixels().x,
				-(float)(floor * 36)});
			const double blue = 100.0, yellow = 230.0; // PPDF stress limits
			sf::Color c = p->stress >= yellow ? sf::Color(255, 0, 0) : p->stress >= blue ? sf::Color(255, 128, 128) : sf::Color::White;
			y.setColor(tinted ? game->lighting.compose(c) : c);
			target.draw(y);
			x += (stepping ? 16 : 8 * yoot.width);
			if (x >= width) break;
			continue;
		}
		// Calculate the texture subrect for this person.
		int type = p->type;
		sf::IntRect sr({type * 32, (stepping ? 48 : 0)}, {p->getWidth(), 24});
		if (direction == Elevator::kDown)
		{
			sr.position.x += 16;
		}

		// Calculate the person's position.
		sf::Vector2f pos;
		pos.y = -(floor * 36);
		pos.x = (direction == Elevator::kUp ? -x : elevator->size.x * 8 + x);
		pos.x += elevator->getPositionPixels().x;

		// The bank is 0..300: pink at 80, red at the 150 judge bar.
		sf::Color color = sf::Color::Black;
		if (p->stress >= 150.0)
			color = sf::Color(255, 0, 0);
		else if (p->stress >= 80.0)
			color = sf::Color(255, 128, 128);
		if (tinted)
		{
			color = game->lighting.compose(color);
			if (statusTint != sf::Color::White)
			{
				color.r = (color.r * statusTint.r) / 255;
				color.g = (color.g * statusTint.g) / 255;
				color.b = (color.b * statusTint.b) / 255;
				color.a = (color.a * statusTint.a) / 255;
			}
		}

		// Draw the person.
		s.setColor(color);
		s.setTextureRect(sr);
		s.setPosition(pos);
		target.draw(s);

		// Move the queue and abort if we've reached our max drawing width.
		x += (stepping ? 16 : p->getWidth());
		if (x >= width)
			break;
	}
}
