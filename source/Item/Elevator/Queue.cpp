#include <algorithm>
#include <cmath>
#include "../../Application.h"
#include "../../Game.h"
#include "../../Person.h"
#include "../../Sprite.h"
#include "Elevator.h"
#include "Queue.h"

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
	  steppingInside(false)
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

bool Queue::addPerson(Person *p)
{
	if (!p) return false;
	if ((int)people.size() >= kQueueCapacity)
	{
		if (!elevator->isService() && !p->isServicePerson())
			p->addJourneyStress(kQueueFullPenalty);
		p->journey.queueRejected();
		return false;
	}
	p->waitStartFrame = game->time.absolute * Time::kSimFramesPerDay;
	people.push_back(p);
	if (!called) callElevator();
	return true;
}

void Queue::removePerson(Person *p)
{
	people.remove(p);
	if (people.empty()) {
		called = answered = steppingInside = false;
		assignedCar = nullptr;
	}
}

void Queue::bankWait(Person *p)
{
	if (elevator->isService()) return;
	const double waited = std::min(300.0, std::max(0.0,
		game->time.absolute * Time::kSimFramesPerDay - p->waitStartFrame));
	const int height = game->mainLobby && floor == 0 ? game->mainLobby->size.y : 1;
	const double bonus = height >= 3 ? 50.0 : height == 2 ? 25.0 : 0.0;
	p->addJourneyStress(std::max(0.0, waited - bonus));
}

Person *Queue::popPerson()
{
	if (people.empty()) return nullptr;
	Person *p = people.front();
	bankWait(p);
	removePerson(p);
	return p;
}

void Queue::callElevator()
{
	if (!called && !people.empty()) {
		called = true;
		answered = false;
		assignedCar = nullptr;
		callTime = game->time.absolute;
		elevator->called(this);
	}
}

void Queue::applyWatchdog()
{
	const double frame = game->time.absolute * Time::kSimFramesPerDay;
	bool ejected = false;
	for (auto it = people.begin(); it != people.end();) {
		Person *p = *it++;
		if (frame - p->waitStartFrame + 1e-7 < kQueueWatchdogFrames) continue;
		bankWait(p);
		p->journey.failWaiting();
		ejected = true;
	}
	if (ejected) {
		called = !people.empty();
		if (people.empty()) {
			answered = steppingInside = false;
			assignedCar = nullptr;
		}
	}
}

void Queue::advance(double dt)
{
	applyWatchdog();
	if (!called && !people.empty()) callElevator();
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
