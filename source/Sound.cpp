#include <cassert>
#include "Game.h"
#include "Sound.h"

using namespace OT;

// ...existing code...

namespace OT {
void Sound::setLoop(bool loop, Game * game)
{
	if (!game || !game->app.soundEnabled) {
		//if (game && game->app.logger.getLevel() <= Logger::DEBUG)
			//LOG(DEBUG, "Sound::setLoop called but sound is disabled");
		return;
	}
	if (!getBuffer()) {
		//if (game->app.logger.getLevel() <= Logger::DEBUG)
			//LOG(DEBUG, "Sound::setLoop called but no buffer is set");
		return;
	}
	sf::Sound::setLoop(loop);
}
}

Sound::Sound()
{
	playingInGame = NULL;
}

void Sound::Play(Game * game)
{
	assert(game);
	if (!game->app.soundEnabled) {
		//if (game->app.logger.getLevel() <= Logger::DEBUG)
			//LOG(DEBUG, "Sound::Play called but sound is disabled");
		return;
	}
	if (!getBuffer()) {
		//if (game->app.logger.getLevel() <= Logger::DEBUG)
			//LOG(DEBUG, "Sound::Play called but no buffer is set");
		return;
	}
	playingInGame = game;
	game->playingSounds.insert(this);
	sf::Sound::play();
}

void Sound::Stop()
{
	if (playingInGame && !playingInGame->app.soundEnabled) {
		if (playingInGame->app.logger.getLevel() <= Logger::DEBUG)
			LOG(DEBUG, "Sound::Stop called but sound is disabled");
		return;
	}
	if (!getBuffer()) {
		if (playingInGame && playingInGame->app.logger.getLevel() <= Logger::DEBUG)
			LOG(DEBUG, "Sound::Stop called but no buffer is set");
		return;
	}
	sf::Sound::stop();
	if (playingInGame) {
		playingInGame->playingSounds.erase(this);
		playingInGame = NULL;
	}
}

double Sound::getDurationDouble()
{
	return getBuffer()->getDuration().asSeconds();
}
