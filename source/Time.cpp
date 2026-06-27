#include <cassert>
#include "Time.h"

using namespace OT;

const static double kDayDuration = 3.2; // minutes / game day
const double Time::kBaseSpeed    = 1.0 / 60 / kDayDuration; // game days / second

// Time-constant (seconds) for the speed ramp. With a frame-rate-independent
// exponential approach this converges ~63% in tau seconds regardless of fps,
// eliminating the rubberbanding the old dt-weighted blend produced.
const static double kSpeedRampTau = 0.25;


double Time::absoluteToHour(double a)
{
	if (a == 1.0)
		return 24.0;

	//jcranmer did some research on how time progresses in SimTower (doc/simtower/TDT_format.txt).
	//We scale the time up to the 2600 frames/day he proposed, and derive the hour by applying
	//different hours/frame scaling factors according to the frame.
	double stage = fmod(a*2600, 2600); //"frame" in TDT_format.txt
	double hour;
	if      (stage < 100)  hour = 0  + (stage-   0)/100 *1;
	else if (stage < 300)  hour = 1  + (stage- 100)/200 *6;
	else if (stage < 700)  hour = 7  + (stage- 300)/400 *5;
	else if (stage < 1500) hour = 12 + (stage- 700)/800 *1;
	else if (stage < 2600) hour = 13 + (stage-1500)/1100*11;
	assert(hour >= 0 && hour < 24);
	return hour;
}

double Time::hourToAbsolute(double h)
{
	if (h == 24.0)
		return 1.0;

	double a;
	if      (h < 1)  a = (h -  0) /  1 *  100 +    0;
	else if (h < 7)  a = (h -  1) /  6 *  200 +  100;
	else if (h < 12) a = (h -  7) /  5 *  400 +  300;
	else if (h < 13) a = (h - 12) /  1 *  800 +  700;
	else if (h < 24) a = (h - 13) / 11 * 1100 + 1500;
	a /= 2600;
	assert(a >= 0 && a <= 1);
	return a;
}

Time::Time()
{
	absolute = 0;
	dta = 0;
	hour = 0;
	day = 0;
	quarter = 1;
	year = 1;
	prev_absolute = 0;
	prev_hour = 0;
	prev_day = 0;
	prev_quarter = 1;
	prev_year = 1;
	speed = 1;
	speed_animated = speed;
}

void Time::set(double t)
{
	if (absolute != t) {
		prev_absolute = absolute;
		prev_hour = hour;
		prev_day = day;
		prev_quarter = quarter;
		prev_year = year;
		
		absolute = t;
		dta = absolute - prev_absolute;
		hour = absoluteToHour(absolute);
		day = (int)floor(t) % 3;
		quarter = (int)floor(t/3) % 4 + 1;
		year = (int)floor(t/3/4) + 1;
	} else {
		dta = 0;
	}
}

void Time::advance(double dt)
{
	// Frame-rate-independent exponential ramp toward the target speed.
	// The old formula (speed*dt + sa*0.1)/(dt+0.1) coupled the convergence
	// rate to the frame delta, so hitches and vsync drift made the apparent
	// game speed oscillate ("rubberband"). (1 - exp(-dt/tau)) is the exact
	// discrete-time first-order filter and is identical for any fixed fps.
	if (speed_animated != speed) {
		if (speed < 1) {
			speed_animated = speed; // pause is instant
		} else {
			double alpha = 1.0 - exp(-dt / kSpeedRampTau);
			speed_animated += (speed - speed_animated) * alpha;
			if (fabs(speed_animated - speed) < 1e-2) speed_animated = speed;
		}
	}

	// No night multiplier: absoluteToHour already maps absolute time
	// non-linearly (night hours occupy far fewer frames than daytime ones),
	// so night visibly passes faster without a hard 2x binary switch that
	// caused the jarring speed jump at 19:00 / 07:00.
	set(absolute + dt * kBaseSpeed * speed_animated);
}

/** Returns the current time in a fraction of hours in a day. */
double Time::getHour()
{
	return (absoluteToHour(absolute));
}

/** Returns true if the time has just passed said absolute time. */
bool Time::check(double a)
{
	return (prev_absolute < a && absolute >= a);
}

/** Returns true if the time hast just passed a multiple of the given period p. E.g. check(1/10.0)
 * will return true 10 times per day. */
bool Time::checkTick(double p)
{
	return (floor(prev_absolute / p) != floor(absolute / p));
}

/** Returns true if the time has just passed said hour. E.g. if the time advanced from 9.98 to 10.1
 * checkHour(10) would return true. */
bool Time::checkHour(double h)
{
	return (prev_hour < h && hour >= h);
}
