#include "Escalator.h"

using namespace OT::Item;

void Escalator::init()
{
	frameCount = 8;
	sprite.setTexture(App->bitmaps["simtower/escalator"]);
	Stairlike::init();
}
