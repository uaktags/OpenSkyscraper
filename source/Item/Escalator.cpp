#include "Escalator.h"

using namespace OT::Item;

void Escalator::init()
{
	frameCount = 8;
	sprite.setTexture(App->bitmaps.editionTexture("escalator", "simtower/escalator"));
	Stairlike::init();
}
