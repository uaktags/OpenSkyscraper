#include "Application.h"
#include "SoundManager.h"
#include "DataManager.h"
#include <SFML/Audio/SoundChannel.hpp>
#include <cstdint>
#include <vector>

using namespace OT;

static bool loadFallbackSound(sf::SoundBuffer &dst)
{
	std::vector<std::int16_t> samples(2205, 0);
	std::vector<sf::SoundChannel> channels;
	channels.push_back(sf::SoundChannel::Mono);
	return dst.loadFromSamples(samples.data(), samples.size(), 1, 44100, channels);
}

bool SoundManager::load(Path name, sf::SoundBuffer & dst)
{
	DataManager::Paths paths = app->data.paths(Path("sounds") + name);
	if (name.str().find(internal_path) == 0) {
		DataManager::Paths strippedPaths = app->data.paths(Path("sounds") + name.str().substr(internal_path.size()));
		paths.insert(paths.end(), strippedPaths.begin(), strippedPaths.end());
	}
	paths.push_back(name);
	
	bool success = false;
	for (int i = 0; i < paths.size() && !success; i++) {
		success = dst.loadFromFile(paths[i].c_str());
		if (!success && paths[i].str().find('.') == std::string::npos) {
			success = dst.loadFromFile((paths[i].str() + ".wav").c_str());
		}
	}
	
	if (success) {
		LOG(DEBUG,   "loaded sound '%s'", name.c_str());
	} else {
		success = loadFallbackSound(dst);
		usedFallback = true;
		LOG(WARNING, "using fallback sound '%s'", name.c_str());
	}
	return success;
}
