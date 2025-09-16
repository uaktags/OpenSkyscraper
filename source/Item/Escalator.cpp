/* Copyright © 2012 Fabian Schuiki */
/* Copyright © 2025 Tim G */
#include "Escalator.h"

using namespace OT::Item;


void Escalator::init()
{
	frameCount = 8;
	sprite.setTexture(App->bitmaps["simtower/escalator"]);
	Stairlike::init();

	LOG(INFO, "Bitmap loaded for %s: simtower/escalator", prototype->name.c_str());
}