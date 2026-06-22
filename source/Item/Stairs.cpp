#include "Stairs.h"
#include "../Game.h"
#include "../Item/Factory.h"

using namespace OT::Item;

void Stairs::init()
{
	configureForLobby();
	Stairlike::init();
}

void Stairs::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Stairlike::encodeXML(xml);
	if (frameCount == 11) {
		xml.PushAttribute("spiral", size.y == 4 ? 3 : 2);
	}
}

void Stairs::decodeXML(tinyxml2::XMLElement & xml)
{
	Stairlike::decodeXML(xml);
	int spiral = xml.IntAttribute("spiral", 0);
	if (spiral == 2) {
		frameCount = 11;
		size.y = 3;
		sprite.setTexture(App->bitmaps["simtower/stairs/spiral_2"]);
	} else if (spiral == 3) {
		frameCount = 11;
		size.y = 4;
		sprite.setTexture(App->bitmaps["simtower/stairs/spiral_3"]);
	} else if (spiral == 0) {
		configureForLobby();
	}
}

void Stairs::configureForLobby()
{
	int lobbyHeight = 0;
	Game::ItemSetByInt::const_iterator it = game->itemsByFloor.find(position.y);
	if (it != game->itemsByFloor.end()) {
		const Game::ItemSet &items = it->second;
		for (Game::ItemSet::const_iterator i = items.begin(); i != items.end(); ++i) {
			Item * item = *i;
			if (item->prototype->icon == ICON_LOBBY && item->getRect().containsPoint(position)) {
				lobbyHeight = item->size.y;
				break;
			}
		}
	}

	if (lobbyHeight == 2) {
		frameCount = 11;
		size.y = 3;
		sprite.setTexture(App->bitmaps["simtower/stairs/spiral_2"]);
	} else if (lobbyHeight >= 3) {
		frameCount = 11;
		size.y = 4;
		sprite.setTexture(App->bitmaps["simtower/stairs/spiral_3"]);
	} else {
		frameCount = 14;
		sprite.setTexture(App->bitmaps["simtower/stairs"]);
	}
}

