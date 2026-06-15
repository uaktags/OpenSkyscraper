#include <cassert>
#include "Game.h"
#include "Sound.h"
#include "Application.h"

using namespace OT;

namespace
{
sf::SoundBuffer &getDefaultSoundBuffer()
{
	static sf::SoundBuffer buffer;
	return buffer;
}
}

Sound::Sound()
	: sf::Sound(getDefaultSoundBuffer())
{
	playingInGame = NULL;
}

void Sound::Play(Game * game)
{
	assert(game);
	playingInGame = game;
	game->playingSounds.insert(this);
	if (OT::App) {
		setVolume(OT::App->audioMuted ? 0.f : OT::App->audioVolume);
	}
	sf::Sound::play();
}

void Sound::Stop()
{
	sf::Sound::stop();
	if (playingInGame) {
		playingInGame->playingSounds.erase(this);
		playingInGame = NULL;
	}
}

double Sound::getDurationDouble()
{
	return getBuffer().getDuration().asSeconds();
}
