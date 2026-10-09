#include <algorithm>
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
    while (!people.empty()) removePerson(*people.begin());
    while (!queues.empty()) {
        Queue *queue = *queues.begin();
        queues.erase(queues.begin());
        delete queue;
    }
	clearCars();
}

void Elevator::resetSchedules()
{
	for (int day = 0; day < kScheduleDays; ++day)
		for (int period = 0; period < kSchedulePeriodCount; ++period)
		{
			schedules[day][period].mode = kScheduleFull;
			schedules[day][period].threshold = 5;
			schedules[day][period].patience = 0;
		}
}

int Elevator::schedulePeriod(double absolute)
{
	const double stage = std::fmod(std::floor(absolute * Time::kSimFramesPerDay + 1e-7),
	                               Time::kSimFramesPerDay);
	const int frame = (int)std::fmod(stage - 300 + Time::kSimFramesPerDay,
	                               Time::kSimFramesPerDay);
	return std::min(6, frame / 400);
}

int Elevator::scheduleMode() const
{
	const int day = game->time.isHoliday() ? 1 : 0;
	return schedules[day][schedulePeriod(game->time.absolute)].mode;
}

bool Elevator::hasWork(const Car *car) const
{
	if (!car->passengers.empty()) return true;
	for (auto *q : queues)
		if (!q->people.empty() && q->assignedCar == car) return true;
	return false;
}

int Elevator::scheduleThreshold() const
{
	const int day = (game->time.isHoliday()) ? 1 : 0;
	const int threshold = schedules[day][schedulePeriod(game->time.absolute)].threshold;
	return std::max(0, std::min(5, threshold));
}

int Elevator::schedulePatienceTicks() const
{
	const int day = (game->time.isHoliday()) ? 1 : 0;
	const int patience = schedules[day][schedulePeriod(game->time.absolute)].patience;
	return std::max(0, std::min(3, patience)) * 30;
}

bool Elevator::canResizeTo(int bottom, int height) const
{
	if (height < 1) return false;
	if (!isExpress() && height > kNonExpressMaxSpan) return false;
	if (bottom <= -10) return false;
	if (game->metroStation && bottom < game->metroStation->position.y - kMetroClearanceFloors)
		return false;

	Game::ItemSetByString::const_iterator group = game->itemsByType.find("elevator");
	if (group == game->itemsByType.end()) return true;
	const int top = bottom + height - 1; // Inclusive top floor.
	for (Game::ItemSet::const_iterator it = group->second.begin(); it != group->second.end(); ++it)
	{
		const Elevator *other = static_cast<const Elevator *>(*it);
		if (other == this) continue;
		const bool horizontal = position.x - kShaftClearanceCells < other->position.x + other->size.x &&
		                        position.x + size.x + kShaftClearanceCells > other->position.x;
		const bool vertical = bottom - 2 < other->position.y + other->size.y &&
		                      top + 1 >= other->position.y;
		if (horizontal && vertical) return false;
	}
	return true;
}

bool Elevator::canAddCar(int floor) const
{
	return (int)cars.size() < kMaxCarsPerShaft &&
	       floor >= position.y && floor < position.y + size.y;
}

void Elevator::reassignCalls(Car *car)
{
	if (!car) return;
	const int floor = (int)std::lround(car->altitude);
	for (auto *q : queues) {
		if (q->floor != floor || q->people.empty()) continue;
		if (scheduleMode() == kScheduleFull && q->direction != car->direction) continue;
		q->assignedCar = car;
		q->called = q->answered = true;
	}
}

void Elevator::finishStop(Car *car)
{
	for (auto *q : queues) {
		if (q->assignedCar != car || q->floor != (int)std::lround(car->altitude)) continue;
		q->assignedCar = nullptr;
		q->answered = false;
		q->called = !q->people.empty();
	}
}

void Elevator::init()
{
	layer = 1;
	maxCarAcceleration = 7.5;
	maxCarSpeed = 10.0;
	maxCarCapacity = isExpress() ? 42 : 21;
	resetSchedules();

	Item::init();

	animation = 0;
	frame = 0;

	const char *shaftEdition = shaftBitmap.find("wide") != std::string::npos
	                         ? "elevator_wide" : "elevator_narrow";
	shaft.setTexture(app->bitmaps.editionTexture(shaftEdition, shaftBitmap.c_str()));
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

		// Red floor plate: row 1 of the digits sheet (the 0x87ec twin of
		// 0x87e9) lights while a car of this group is on this floor
		// (IsCarOnFloor 10a8:367). Replaces the old home-floor tint.
		bool carOnFloor = false;
		for (auto *c : cars) {
			if (std::lround(c->altitude) == flr) {
				carOnFloor = true;
				break;
			}
		}
		const int digitRow = carOnFloor ? 1 : 0;
		if (tinted)
		{
			sf::Color composed = game->lighting.compose(sf::Color::White);
			if (statusTint != sf::Color::White)
			{
				composed.r = (composed.r * statusTint.r) / 255;
				composed.g = (composed.g * statusTint.g) / 255;
				composed.b = (composed.b * statusTint.b) / 255;
				composed.a = (composed.a * statusTint.a) / 255;
			}
			d.setColor(composed);
		}

		char c[8];
		int len = snprintf(c, 8, "%i", flr);
		int x = size.x * 4 - (len - 1) * 6;
		for (int i = 0; i < len; i++)
		{
			int p = 10;
			if (c[i] >= '0' && c[i] <= '9')
				p = c[i] - '0';
			d.setTextureRect(sf::IntRect({p * 11, digitRow * 17}, {11, 17}));
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
	for (std::set<int>::iterator i = unservicedFloors.begin(); i != unservicedFloors.end(); ++i)
	{
		xml.OpenElement("unserviced");
		xml.PushAttribute("floor", *i);
		xml.CloseElement();
	}
	for (int day = 0; day < kScheduleDays; ++day)
	{
		xml.OpenElement("schedule");
		xml.PushAttribute("day", day);
		for (int period = 0; period < kSchedulePeriodCount; ++period)
		{
			xml.OpenElement("period");
			xml.PushAttribute("index", period);
			xml.PushAttribute("mode", schedules[day][period].mode);
			xml.PushAttribute("threshold", schedules[day][period].threshold);
			xml.PushAttribute("patience", schedules[day][period].patience);
			xml.CloseElement();
		}
		xml.CloseElement();
	}
	for (Cars::iterator c = cars.begin(); c != cars.end(); ++c)
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
	size.y = xml.IntAttribute("height", size.y);
	showShaft = xml.BoolAttribute("showShaft", true);
	resetSchedules();
	for (tinyxml2::XMLElement *s = xml.FirstChildElement("schedule"); s; s = s->NextSiblingElement("schedule"))
	{
		const int day = std::max(0, std::min(kScheduleDays - 1, s->IntAttribute("day", 0)));
		for (tinyxml2::XMLElement *p = s->FirstChildElement("period"); p; p = p->NextSiblingElement("period"))
		{
			const int index = p->IntAttribute("index", -1);
			if (index < 0 || index >= kSchedulePeriodCount) continue;
			schedules[day][index].mode = std::max(0, std::min(2, p->IntAttribute("mode", 0)));
			schedules[day][index].threshold = std::max(0, std::min(5, p->IntAttribute("threshold", 5)));
			schedules[day][index].patience = std::max(0, std::min(3, p->IntAttribute("patience", 0)));
		}
	}
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("unserviced"); e; e = e->NextSiblingElement("unserviced"))
		unservicedFloors.insert(e->IntAttribute("floor"));
	for (tinyxml2::XMLElement *e = xml.FirstChildElement("car"); e; e = e->NextSiblingElement("car"))
	{
		if ((int)cars.size() >= kMaxCarsPerShaft) break;
		Car *car = new Car(this);
		car->index = (int)cars.size();
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
		height = size.y + position.y - newy;
	}
	else
	{
		newy = position.y;
		height = y - position.y;
	}
	if (height < 1) height = 1;
	if (!isExpress() && height > kNonExpressMaxSpan) {
		game->timeWindow.showMessage("Elevator shaft can cover only 30 floors");
		return false;
	}
	if (!canResizeTo(newy, height)) return false;
	if (newy == position.y && height == size.y) return false;

	setPosition(int2(position.x, newy));
	size.y = height;
	for (Cars::iterator c = cars.begin(); c != cars.end(); ++c)
	{
		Car *car = *c;
		if (car->altitude < newy) car->setAltitude(newy);
		else if (car->altitude >= newy + height) car->setAltitude(newy + height - 1);
		if (car->destinationFloor < newy) car->destinationFloor = newy;
		else if (car->destinationFloor >= newy + height) car->destinationFloor = newy + height - 1;
		if (car->state == Car::kIdle) car->startAltitude = car->altitude;
		car->reposition();
	}

	const int maxY = newy + height;
	for (std::set<int>::iterator i = unservicedFloors.begin(); i != unservicedFloors.end();)
	{
		if (*i < newy || *i >= maxY) unservicedFloors.erase(i++);
		else ++i;
	}
	updateSprite();
	cleanQueues();
	return true;
}

void Elevator::clearCars()
{
	for (Cars::iterator c = cars.begin(); c != cars.end(); ++c)
		delete *c;
	cars.clear();
}

void Elevator::addCar(int floor)
{
	if (!canAddCar(floor)) return;
	Car *car = new Car(this);
	car->index = (int)cars.size();
	car->setAltitude(floor);
	car->startAltitude = floor;
	car->destinationFloor = floor;
	car->homeFloor = floor;
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
	Direction dir = p->journey.toFloor > p->journey.fromFloor ? kUp : kDown;
	if (!getQueue(p->journey.fromFloor, dir)->addPerson(p)) return;
	if (p->at) p->at->removePerson(p);
	p->journey.queueAccepted();
	Item::addPerson(p);
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

void Elevator::called(Queue *queue)
{
	if (queue && !queue->people.empty()) respondToCalls();
}

void Elevator::respondToCalls()
{
	while (Queue *q = getMostUrgentQueue())
	{
		Car *idle = nullptr, *approaching = nullptr, *reversing = nullptr;
		double idleCost = 1e30, approachCost = 1e30, reverseCost = 1e30;
		std::vector<Car *> ordered(cars.begin(), cars.end());
		std::sort(ordered.begin(), ordered.end(), [](Car *a, Car *b) { return a->index < b->index; });
		for (auto *c : ordered)
		{
			const bool parked = !hasWork(c) && c->state == Car::kIdle && c->doorTicks == 0 &&
			                    std::abs(c->altitude - c->homeFloor) < 0.01;
			const double toward = (q->floor - c->altitude) * c->direction;
			if (parked) {
				const double cost = std::abs(c->altitude - q->floor);
				if (cost < idleCost) { idle = c; idleCost = cost; }
			} else if (c->direction == q->direction && toward >= 0) {
				if (toward < approachCost) { approaching = c; approachCost = toward; }
			} else {
				double turn = c->altitude;
				auto consider = [&](int floor) {
					if ((floor - c->altitude) * c->direction >= 0 &&
					    std::abs(floor - c->altitude) > std::abs(turn - c->altitude)) turn = floor;
				};
				for (auto *p : c->passengers) consider(p->journey.toFloor);
				for (auto *call : queues)
					if (!call->people.empty() && call->assignedCar == c) consider(call->floor);
				const double cost = std::abs(c->altitude - turn) + std::abs(q->floor - turn);
				if (cost < reverseCost) { reversing = c; reverseCost = cost; }
			}
		}
		Car *work = approaching ? approaching : reversing;
		const double workCost = approaching ? approachCost : reverseCost;
		Car *car = !idle ? work : !work ? idle :
		           workCost - idleCost < scheduleThreshold() ? work : idle;
		if (!car) break;
		q->assignedCar = car;
		q->answered = true;
		if (car->state == Car::kIdle) {
			car->direction = q->direction;
			car->shuttleMode = scheduleMode();
			car->shuttleReturning = false;
			// At the calling floor an idle car boards before starting its shuttle.
			if (std::abs(car->altitude - q->floor) < 0.01) car->moveTo(q->floor);
			else decideCarDestination(car);
		} else if (car->state == Car::kMoving && car->doorTicks == 0)
			decideCarDestination(car);
	}
}

Queue *Elevator::getMostUrgentQueue()
{
	Queue *queue = nullptr;
	for (auto *q : queues) {
		if (q->people.empty() || !q->called || q->answered) continue;
		if (!queue || q->getWaitDuration() > queue->getWaitDuration()) queue = q;
	}
	return queue;
}

Car *Elevator::getIdleCar(int floor)
{
	Car *best = nullptr;
	for (auto *c : cars) {
		if (hasWork(c) || c->state != Car::kIdle || c->doorTicks != 0 ||
		    std::abs(c->altitude - c->homeFloor) >= 0.01) continue;
		const double cost = std::abs(c->altitude - floor);
		if (!best || cost < std::abs(best->altitude - floor) ||
		    (cost == std::abs(best->altitude - floor) && c->index < best->index)) best = c;
	}
	return best;
}

void Elevator::decideCarDestination(Car *car)
{
	if (!car) return;
	const int floor = (int)std::lround(car->altitude);
	if (!hasWork(car)) {
		car->shuttleReturning = false;
		car->shuttleMode = scheduleMode();
		car->doorTicks = 0;
		if (floor != car->homeFloor) car->moveTo(car->homeFloor);
		else car->setState(Car::kIdle);
		return;
	}
	const int mode = scheduleMode();
	if (mode != car->shuttleMode) {
		car->shuttleMode = mode;
		car->shuttleReturning = false;
	}
	if (mode != kScheduleFull) {
		const int outbound = mode == kScheduleExpressUp ? 1 : -1;
		const int extreme = outbound > 0 ? position.y + size.y - 1 : position.y;
		if (!car->shuttleReturning && floor != extreme) {
			car->moveTo(extreme);
			return;
		}
		car->shuttleReturning = true;
		const int serve = -outbound;
		int target = INT_MAX;
		auto consider = [&](int f) {
			if ((f - floor) * serve <= 0) return;
			if (target == INT_MAX || std::abs(f - floor) < std::abs(target - floor)) target = f;
		};
		for (auto *p : car->passengers) consider(p->journey.toFloor);
		if (!car->isFull()) for (auto *q : queues)
			if (!q->people.empty() && q->assignedCar == car) consider(q->floor);
		if (target != INT_MAX) { car->moveTo(target); return; }
		const int end = serve > 0 ? position.y + size.y - 1 : position.y;
		if (floor != end) { car->moveTo(end); return; }
		car->shuttleReturning = false;
		car->moveTo(extreme);
		return;
	}
	// SCAN: passenger stops and same-direction assigned calls first;
	// opposite calls become the farthest reversal point of the sweep.
	for (int pass = 0; pass < 2; ++pass) {
		const int dir = car->direction == kNone ? (pass == 0 ? 1 : -1) :
		                (pass == 0 ? car->direction : -car->direction);
		int target = INT_MAX, opposite = INT_MAX;
		auto consider = [&](int f) {
			if ((f - floor) * dir <= 0) return;
			if (target == INT_MAX || std::abs(f - floor) < std::abs(target - floor)) target = f;
		};
		for (auto *p : car->passengers) consider(p->journey.toFloor);
		if (!car->isFull()) for (auto *q : queues) {
			if (q->people.empty() || q->assignedCar != car || (q->floor - floor) * dir < 0) continue;
			if (q->floor == floor) {
				car->direction = q->direction; car->destinationFloor = floor;
				car->doorTicks = 5; car->setState(Car::kHauling); return;
			}
			if (q->direction == dir) consider(q->floor);
			else if (opposite == INT_MAX || std::abs(q->floor - floor) > std::abs(opposite - floor))
				opposite = q->floor;
		}
		if (target == INT_MAX) target = opposite;
		if (target != INT_MAX) {
			car->moveTo(target);
			// Hall direction at a reversal stop is selected when doors open.
			return;
		}
	}
	car->setState(Car::kIdle);
}

void Elevator::evacuateForDisaster()
{
 // Game parked passengers through virtual removePerson before this hook.
 while (!queues.empty()) {
  Queue *q = *queues.begin(); queues.erase(queues.begin());
  q->people.clear(); delete q;
 }
 for (Car *c : cars) {
  c->passengers.clear();
  c->setAltitude(c->homeFloor);
  c->startAltitude = c->homeFloor;
  c->destinationFloor = c->homeFloor;
  c->direction = kNone;
  c->state = Car::kIdle;
  c->journeyTime = 0;
  c->doorTicks = c->holdTicks = c->moveTicks = 0;
  c->arrivingPlayed = c->departingPlayed = false;
  c->arrivingSound.Stop(); c->departingSound.Stop(); c->updateSprite();
 }
}
