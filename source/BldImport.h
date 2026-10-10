/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include <string>

namespace OT
{
	class Game;

	/**
	 * Loads a YootTower `.bld` save into `game` (BldLoader parse + native
	 * XML conversion + Game::decodeXML), logging what was imported and what
	 * had no counterpart. The .bld itself is never written: saveFilename is
	 * pointed at a sibling `.tower`. Returns false (game untouched) when the
	 * file does not parse.
	 */
	bool importBldSave(Game & game, const std::string & path);
}
