/* Copyright © 2013 Fabian Schuiki */
#pragma once
#include "Sprite.h"
#include "GameObject.h"
#include <map>
#include <SFML/Graphics/Drawable.hpp>

namespace OT {
	class Game;

	class Decorations : public GameObject, public sf::Drawable
	{
	public:
		Decorations(Game * game);

		void reset();
		void updateFloor(int y);
		void updateCrane();
		void updateTracks();
		void reloadTextures();

		void draw(sf::RenderTarget & target, sf::RenderStates states) const;
		virtual void render(sf::RenderTarget & target) const;

	private:
		struct FireStairPair {
			Sprite minX;
			Sprite maxX;
		};
		typedef std::map<int, FireStairPair> FireStairs;
		FireStairs fireStairs;

		Sprite crane;
		bool craneVisible;
		// OverlayT crane state (UpdateCrane 11c0:024a): crane_x is captured
		// only when the top-floor NUMBER changes and then stays put even if
		// the floor widens — the original's famous stuck crane. Draw gate
		// (11c0:0000): hidden at/above displayed floor 100 or under the
		// cathedral crown.
		static const int kNoCraneFloor = -1000; // sentinel; basements are negative
		static const int kCraneMinCells = 7;    // top floor must span >= 7 cells
		static const int kCraneMaxFloor = 100;  // floor-100 cap hides the crane
		int craneFloor;
		int craneX;

		Sprite track;
		bool tracksVisible;
	};
}
