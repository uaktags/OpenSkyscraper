#include "Application.h"
#include "Game.h"
#include "GameObject.h"

using namespace OT;

static unsigned long long s_nextSimSerial = 0;

GameObject::GameObject(Game * game)
:	game(game),
	app(&game->app),
	simSerial(s_nextSimSerial++)
{
}

GameObject::GameObject(const GameObject & other)
:	game(other.game),
	app(other.app),
	simSerial(s_nextSimSerial++)
{
}
