#include <cassert>
#include <vector>
#include "../../Game.h"
#include "../../Sprite.h"
#include "Elevator.h"
#include "Car.h"
#include "Queue.h"
#include <cmath>

using namespace OT;
using namespace OT::Item::Elevator;

Elevator::~Elevator()
{
	clearCars();
}

void Elevator::init()
{
	layer = 1;
	maxCarAcceleration = 7.5;
	maxCarSpeed = 10.0;
	maxCarCapacity = 21;

	Item::init();

	animation = 0;
	frame = 0;

	shaft.setTexture(app->bitmaps[shaftBitmap]);
	shaft.setTextureRect(sf::IntRect({0, 0}, {size.x * 8, 36}));
	shaft.setOrigin({0.f, 36.f});
	shaft.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-position.y * 36)});

	topMotor.setTexture(shaft.getTexture());
	topMotor.setOrigin({0.f, 36.f});
	bottomMotor.setTexture(shaft.getTexture());
	bottomMotor.setOrigin({0.f, 36.f});

	addSprite(&topMotor);
	addSprite(&bottomMotor);

	updateSprite();

	addCar(position.y);
}
void Elevator::updateSprite()
{
	int w = getSizePixels().x;
	int h = getSizePixels().y;

	topMotor.setTextureRect(sf::IntRect({(2 * frame + 1) * w, 0}, {w, 36}));
	bottomMotor.setTextureRect(sf::IntRect({(2 * frame + 2) * w, 0}, {w, 36}));
	topMotor.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-(position.y + size.y) * 36)});
	bottomMotor.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-(position.y - 1) * 36)});
}

void Elevator::render(sf::RenderTarget &target) const
{
	Item::render(target);

	// Draw the elevator floors.
	Sprite s = shaft;
	Sprite d;
	d.setTexture(app->bitmaps["simtower/elevator/digits"]);
	d.setOrigin({5, 8});

	sf::Color statusTint = sf::Color::White;
	if (game->statusMode == Game::kHotel)
	{
		statusTint = sf::Color(110, 110, 110, 160);
	}

	const sf::Color tint = game->lighting.tint();
	const bool tinted = (tint != sf::Color(255, 255, 255, 255) || statusTint != sf::Color::White);
	if (tinted)
	{
		sf::Color composed = game->lighting.compose(s.getColor());
		if (statusTint != sf::Color::White)
		{
			composed.r = (composed.r * statusTint.r) / 255;
			composed.g = (composed.g * statusTint.g) / 255;
			composed.b = (composed.b * statusTint.b) / 255;
			composed.a = (composed.a * statusTint.a) / 255;
		}
		s.setColor(composed);
	}

	int minY = position.y;
	int maxY = size.y + minY - 1;

	// When the shaft is hidden ("Show: No"), draw just the two vertical
	// border rails so the player can still see the shaft outline while
	// tenants behind it show through. Drawn before the per-floor loop so
	// digits and cars render on top.
	if (!showShaft)
	{
		const float railX0 = static_cast<float>(position.x * 8);
		const float railX1 = static_cast<float>((position.x + size.x) * 8 - 1);
		const float railY0 = static_cast<float>(-(position.y + size.y) * 36); // top (screen)
		const float railY1 = static_cast<float>(-position.y * 36);             // bottom (screen)
		const float railH  = railY1 - railY0;
		sf::Color railCol(20, 20, 20);
		if (tinted) railCol = game->lighting.compose(railCol);
		sf::RectangleShape leftRail({1.f, railH});
		leftRail.setPosition({railX0, railY0});
		leftRail.setFillColor(railCol);
		target.draw(leftRail);
		sf::RectangleShape rightRail({1.f, railH});
		rightRail.setPosition({railX1, railY0});
		rightRail.setFillColor(railCol);
		target.draw(rightRail);
	}

	for (int y = minY; y <= maxY; y++)
	{
		d.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-y * 36)});
		if (showShaft)
		{
			s.setPosition({static_cast<float>(position.x * 8), static_cast<float>(-y * 36)});
			target.draw(s);
		}

		int flr = y;
		if (!connectsFloor(flr))
			continue;

		bool isHome = false;
		for (auto *c : cars) {
			if (c->homeFloor == flr) {
				isHome = true;
				break;
			}
		}
		sf::Color digitColor = isHome ? sf::Color(255, 100, 100) : sf::Color::White; // Pinkish-red for home floor, white otherwise
		if (tinted)
		{
			sf::Color composed = game->lighting.compose(digitColor);
			if (statusTint != sf::Color::White)
			{
				composed.r = (composed.r * statusTint.r) / 255;
				composed.g = (composed.g * statusTint.g) / 255;
				composed.b = (composed.b * statusTint.b) / 255;
				composed.a = (composed.a * statusTint.a) / 255;
			}
			d.setColor(composed);
		}
		else
		{
			d.setColor(digitColor);
		}

		char c[8];
		int len = snprintf(c, 8, "%i", flr);
		int x = size.x * 4 - (len - 1) * 6;
		for (int i = 0; i < len; i++)
		{
			int p = 10;
			if (c[i] >= '0' && c[i] <= '9')
				p = c[i] - '0';
			d.setTextureRect(sf::IntRect({p * 11, 0}, {11, 17}));
			d.setPosition({static_cast<float>(position.x * 8 + x), static_cast<float>(-y * 36.f - 10.f)});
			target.draw(d);
			x += 12;
		}
	}

	// Draw the cars and queues.
	for (Cars::iterator c = cars.begin(); c != cars.end(); c++)
		target.draw(**c);
	for (Queues::iterator q = queues.begin(); q != queues.end(); q++)
		target.draw(**q);
}

void Elevator::advance(double dt)
{
	bool carsMoving = false;
	for (Cars::iterator ic = cars.begin(); ic != cars.end(); ic++)
	{
		(*ic)->advance(dt);
		if ((*ic)->state == Car::kMoving)
			carsMoving = true;
	}

	// Advance the queues so people get stressed.
	for (Queues::iterator iq = queues.begin(); iq != queues.end(); iq++)
		(*iq)->advance(dt);

	// Animate the elevator motors if there's a car moving.
	if (carsMoving)
	{
		animation = std::fmod(animation + dt, 1);
		int newFrame = std::floor(animation * 3);
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

void Elevator::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("height", size.y);
	xml.PushAttribute("showShaft", showShaft);
	for (std::set<int>::iterator i = unservicedFloors.begin(); i != unservicedFloors.end(); i++)
	{
		xml.OpenElement("unserviced");
		xml.PushAttribute("floor", *i);
		xml.CloseElement();
	}
	for (Cars::iterator c = cars.begin(); c != cars.end(); c++)
	{
		xml.OpenElement("car");
		(*c)->encodeXML(xml);
		xml.CloseElement();
	}
}

void Elevator::decodeXML(tinyxml2::XMLElement &xml)
{
	clearCars();
	Item::decodeXML(xml);
	size.y = xml.IntAttribute("height");
	showShaft = xml.BoolAttribute("showShaft", true);
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("unserviced"); e; e = e->NextSiblingElement("unserviced"))
	{
		unservicedFloors.insert(e->IntAttribute("floor"));
	}
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("car"); e; e = e->NextSiblingElement("car"))
	{
		Car *car = new Car(this);
		car->decodeXML(*e);
		cars.insert(car);
	}
	updateSprite();
}

/*
 * This is reimplemented because the elevator contains
 * the two "operational" components around it in the
 * vertical direction, and thus has a different mouse
 * region.
 */

rectd Elevator::getMouseRegion()
{
	int2 p = getPositionPixels();
	sf::Vector2u s = getSizePixels();
	return rectd(p.x, p.y - (int)s.y, s.x, s.y * 3);
}

bool Elevator::containsPoint(const double2 & pt)
{
	// Only the motor gearboxes (top and bottom) are clickable, matching
	// SimTower where you grab the black motor box to resize the shaft.
	// Clicks on the shaft interior pass through to the tenant behind.
	sf::FloatRect tb = topMotor.getGlobalBounds();
	rectd topRect(tb.position.x, -tb.position.y - tb.size.y, tb.size.x, tb.size.y);
	if (topRect.containsPoint(pt)) return true;

	sf::FloatRect bb = bottomMotor.getGlobalBounds();
	rectd botRect(bb.position.x, -bb.position.y - bb.size.y, bb.size.x, bb.size.y);
	if (botRect.containsPoint(pt)) return true;

	return false;
}

bool Elevator::repositionMotor(int motor, int y)
{
	assert(motor == -1 || motor == 1);
	int height;
	int newy;
	if (motor == -1)
	{
		newy = y + 1;
		height = (size.y + position.y - newy);
	}
	else
	{
		newy = position.y;
		height = (y - position.y);
	}
	if (height < 1)
		height = 1;
	if (height > 30 + 1)
		height = 30 + 1;
	if (motor == -1)
	{
		newy = (size.y + position.y - height);
	}
	if (newy != position.y || height != size.y)
	{
		setPosition(int2(position.x, newy));
		size.y = height;
		// TODO: constrain cars to stay within elevator bounds.
		for (Cars::iterator c = cars.begin(); c != cars.end(); c++)
		{
			Car *car = *c;
			if (car->altitude < newy)
				car->altitude = newy;
			else if (car->altitude >= newy + height)
				car->altitude = newy + height - 1;

			if (car->destinationFloor < newy)
				car->destinationFloor = newy;
			else if (car->destinationFloor >= newy + height)
				car->destinationFloor = newy + height - 1;

			if (car->state == Car::kIdle)
				car->startAltitude = car->altitude;

			(*c)->reposition();
		}

		int maxY = newy + height;
		for (std::set<int>::iterator i = unservicedFloors.begin(); i != unservicedFloors.end();)
		{
			int floor = *i;
			if (floor < newy || floor >= maxY)
				unservicedFloors.erase(i++);
			else
				i++;
		}
		updateSprite();
		cleanQueues();
		return true;
	}
	return false;
}

void Elevator::clearCars()
{
	for (Cars::iterator c = cars.begin(); c != cars.end(); c++)
		delete *c;
	cars.clear();
}

void Elevator::addCar(int floor)
{
	Car *car = new Car(this);
	car->setAltitude(floor);
	car->startAltitude = floor;
	car->destinationFloor = floor;
	cars.insert(car);
}

bool Elevator::connectsFloor(int floor) const
{
	if (floor < position.y || floor >= position.y + size.y)
		return false;
	return !unservicedFloors.count(floor);
}

void Elevator::addPerson(Person *p)
{
	Item::addPerson(p);
	Direction dir = (p->journey.toFloor > p->journey.fromFloor ? kUp : kDown);
	getQueue(p->journey.fromFloor, dir)->addPerson(p);
}

void Elevator::removePerson(Person *p)
{
	for (Queues::iterator iq = queues.begin(); iq != queues.end(); iq++)
	{
		(*iq)->removePerson(p);
	}
	for (Cars::iterator ic = cars.begin(); ic != cars.end(); ic++)
	{
		(*ic)->removePassenger(p);
	}
	Item::removePerson(p);
}

/// Returns the queue for the given floor and direction, creating it if it does not yet exist.
Queue *Elevator::getQueue(int floor, Direction dir)
{
	for (Queues::iterator iq = queues.begin(); iq != queues.end(); iq++)
	{
		if ((*iq)->floor == floor && (*iq)->direction == dir)
			return *iq;
	}

	// Create the queue as there was none.
	Queue *q = new Queue(this);
	q->floor = floor;
	q->direction = dir;
	q->width = 400;
	queues.insert(q);
	return q;
}

/// Removes all queues on floors that the elevator doesn't connect to anymore. This is necessary
/// after the elevator motors are repositioned or certain floors deactivated.
void Elevator::cleanQueues()
{
	// Collect queues to remove first to avoid iterator invalidation and double-delete.
	std::vector<Queue *> toRemove;
	for (Queues::iterator iq = queues.begin(); iq != queues.end(); ++iq)
	{
		if (!connectsFloor((*iq)->floor))
			toRemove.push_back(*iq);
	}
	for (std::vector<Queue *>::iterator it = toRemove.begin(); it != toRemove.end(); ++it)
	{
		queues.erase(*it);
		delete *it;
	}
}

/// Called whenever the first person queues up at an elevator queue. The elevator is now
/// responsible for making a car answer the call or postpone it.
void Elevator::called(Queue *queue)
{
	assert(queue);
	respondToCalls();
}

/// Responds to the most urgent calls.
void Elevator::respondToCalls()
{
	// Iterate through the queues, always getting the most urgent one.
	Queue *q;
	while ((q = getMostUrgentQueue()))
	{
		// Find the car best suited for responding. If none could be found, abort since all cars
		// are occupied.
		Car *car = getIdleCar(q->floor);
		if (!car)
			break;

		// Answer the call.
		q->answered = true;
		car->direction = q->direction;
		car->moveTo(q->floor);
	}
}

/// Returns the queue that is the most urgent to respond to.
Queue *Elevator::getMostUrgentQueue()
{
	Queue *queue = NULL;
	for (Queues::iterator iq = queues.begin(); iq != queues.end(); iq++)
	{
		if ((*iq)->called && !(*iq)->answered && (!queue || queue->getWaitDuration() < (*iq)->getWaitDuration()))
			queue = *iq;
	}
	return queue;
}

/// Returns the idle car closest to the given floor, or NULL if no car is idle.
Car *Elevator::getIdleCar(int floor)
{
	Car *car = NULL;
	for (Cars::iterator ic = cars.begin(); ic != cars.end(); ic++)
	{
		Car *c = *ic;
		if (c->state == Car::kIdle && (!car || fabs(car->altitude - floor) > fabs(c->altitude - floor)))
			car = c;
	}
	return car;
}

void Elevator::decideCarDestination(Car *car)
{
	// Find the next floor a passenger needs to go.
	int nextFloor = INT_MAX;
	for (People::iterator ip = car->passengers.begin(); ip != car->passengers.end(); ip++)
	{
		int f = (*ip)->journey.toFloor;
		if (nextFloor == INT_MAX || abs(car->destinationFloor - nextFloor) > abs(car->destinationFloor - f))
			nextFloor = f;
	}

	// Find the next queue that would lie in the car's path.
	Queue *nextQueue = NULL;
	double queueDistance = 0;
	for (Queues::iterator iq = queues.begin(); iq != queues.end(); iq++)
	{
		Queue *q = *iq;

		// Skip queues that aren't called, are answered, or are for the opposite direction.
		if (q->direction != car->direction)
			continue;
		if (!q->called || q->answered)
			continue;

		// Calculate the offset of the queue and the car, based on the car's current direction.
		// This will make queues that lie ahead of the car have a positive distance, and cars that
		// lie behin the car a negative distance. We only serve queues that are somewhat ahead of
		// the car.
		double distance = q->floor - car->altitude;
		distance *= car->direction;
		if (distance < 0.5)
			continue;

		// Keep the best queue.
		if (!nextQueue || queueDistance > distance)
		{
			queueDistance = distance;
			nextQueue = q;
		}
	}

	// Move the car to the closer of the two destinations.
	if (nextQueue && !car->isFull())
	{
		double floorDistance = fabs(nextFloor - car->altitude);
		if (queueDistance <= floorDistance)
		{
			nextQueue->answered = true;
			car->moveTo(nextQueue->floor);
			return;
		}
	}
	getQueue(nextFloor, car->direction)->answered = true;
	car->moveTo(nextFloor);
}
