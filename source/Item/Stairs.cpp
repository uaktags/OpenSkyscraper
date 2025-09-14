#include "Stairs.h"

using namespace OT::Item;


void Stairs::init()
{
	frameCount = 14;
	sprite.setTexture(App->bitmaps["simtower/stairs"]);
	Stairlike::init();

	LOG(INFO, "Bitmap loaded for %s: simtower/stairs", prototype->name.c_str());
}
