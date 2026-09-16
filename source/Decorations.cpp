/* Copyright © 2013 Fabian Schuiki */
#include "Decorations.h"
#include "Game.h"
using OT::Decorations;
using OT::Game;


Decorations::Decorations(Game * game)
:	GameObject(game)
{
	// Initialize the crane sprite (36x36, parked at a floor's left edge).
	crane.SetImage(App->bitmaps["simtower/deco/crane"]);
	crane.setOrigin({0.f, 36.f});
	craneVisible = false;
	craneFloor = kNoCraneFloor;
	craneX = 0;

	track.SetImage(App->bitmaps["simtower/metro/tracks"]);
	track.setTextureRect(sf::IntRect({0,0}, {32,36}));
	track.setOrigin({0.f, 36.f});
	tracksVisible = false;
}

void Decorations::reset()
{
	fireStairs.clear();
	craneVisible = false;
	craneFloor = kNoCraneFloor;
	tracksVisible = false;
}

/** Updates the decorations for the given floor. This effectively repositions the fire stairs of
 * the tower on this floor. */
void Decorations::updateFloor(int y)
{
	// If there is no floor item, remove any existing stairs.
	if (game->floorItems.count(y) == 0) {
		FireStairs::iterator i = fireStairs.find(y);
		if (i != fireStairs.end()) {
			fireStairs.erase(i);
			LOG(DEBUG, "Removed fire stairs on floor %i", y);
		}
	}

	// Otherwise calculate the width of the floor.
	else if (y > 0) {
		Item::Item * f = game->floorItems[y];

		//Create a new pair of fire stairs if required.
		bool exists = fireStairs.count(y) == 1;
		FireStairPair &fsp = fireStairs[y];
		if (!exists) {
			fsp.minX.SetImage(App->bitmaps["simtower/deco/fireladder"]);
			fsp.maxX.SetImage(fsp.minX.getTexture());
			fsp.minX.setTextureRect(sf::IntRect({0, 0}, {24, 36}));
			fsp.maxX.setTextureRect(sf::IntRect({24, 0}, {24, 36}));
			fsp.minX.setOrigin({24.f, 36.f});
			fsp.maxX.setOrigin({0.f, 36.f});
			LOG(DEBUG, "Added fire stairs on floor %i", y);
		}

		//Position the fire stairs accordingly.
		int minFloorX = f->getRect().minX();
		int maxFloorX = f->getRect().maxX();
		fsp.minX.setPosition({minFloorX * 8.0f, -y * 36.0f});
		fsp.maxX.setPosition({maxFloorX * 8.0f, -y * 36.0f});
		LOG(DEBUG, "Repositioned fire stairs on floor %i to %i and %i", y, minFloorX, maxFloorX);
	}
}

/** Updates the crane at the top of the tower (OverlayT 11c0). Re-evaluates the crane only when
 * the top floor's NUMBER changes: it then parks at the floor's LEFT edge if the floor spans at
 * least 7 cells, and stays there even if the floor later grows sideways (the original's stuck
 * crane). Hidden at/above floor 100 and under the cathedral crown. */
void Decorations::updateCrane()
{
	//Top floor with non-transport tenants and its extent (OverlayT's
	//floor-map records cover non-transport items only; a floor spanned
	//solely by an elevator or stair does not count).
	int maxY = 0;
	int minFloorX = INT_MAX;
	int maxFloorX = INT_MIN;
	for (Game::ItemSetByInt::const_iterator ii = game->itemsByFloor.begin(); ii != game->itemsByFloor.end(); ii++) {
		const int y = ii->first;
		if (y < maxY)
			continue;
		for (Game::ItemSet::const_iterator ij = ii->second.begin(); ij != ii->second.end(); ij++) {
			Item::Item * i = *ij;
			if (i->canHaulPeople())
				continue;
			if (y > maxY) {
				maxY = y;
				minFloorX = INT_MAX;
				maxFloorX = INT_MIN;
			}
			minFloorX = std::min(minFloorX, i->position.x);
			maxFloorX = std::max(maxFloorX, i->getRect().maxX());
		}
	}

	//UpdateCrane (11c0:024a): re-evaluate only when the top floor NUMBER
	//changes — crane_x stays where it was captured even if the floor
	//later widens (the original's stuck crane). A top floor narrower
	//than 7 cells gets no crane; while absent (sentinel), every call
	//re-checks, so widening spawns the crane at the new left edge.
	if (maxY <= 0) {
		craneFloor = kNoCraneFloor;
	} else if (craneFloor != maxY) {
		if (maxFloorX - minFloorX < kCraneMinCells)
			craneFloor = kNoCraneFloor;
		else {
			craneFloor = maxY;
			craneX = minFloorX;
		}
	}

	//Draw gate (11c0:0000): hidden at/above displayed floor 100 or once
	//the cathedral crowns the tower.
	Game::ItemSetByString::const_iterator cathedral = game->itemsByType.find("cathedral");
	bool hasCathedral = (cathedral != game->itemsByType.end() && !cathedral->second.empty());
	if (craneFloor != kNoCraneFloor && craneFloor < kCraneMaxFloor && !hasCathedral) {
		crane.setPosition({craneX * 8.0f, -(craneFloor + 1) * 36.0f});
		craneVisible = true;
	} else {
		craneVisible = false;
	}
}

void Decorations::updateTracks() {
	if (game->metroStation) {
		track.setPosition({track.getPosition().x, -game->metroStation->position.y * 36.0f});
		tracksVisible = true;
	} else {
		tracksVisible = false;
	}
}

void Decorations::draw(sf::RenderTarget & target, sf::RenderStates states) const
{
	render(target);
}

/** Renders the tower decorations to the given render target. */
void Decorations::render(sf::RenderTarget & target) const
{
	const sf::View &view = target.getView();
	sf::Vector2f center = view.getCenter();
	sf::Vector2f size = view.getSize();
	sf::Vector2f dmax = center + size/2.f;
	sf::Vector2f dmin = center - size/2.f;

	sf::FloatRect rect = target.getView().getViewport();
	for (int y = (int)floor(dmin.y / 36); y <= ceil(dmax.y / 36); y++) {
		FireStairs::const_iterator fsi = fireStairs.find(-y);
		if (fsi == fireStairs.end()) continue;
		target.draw(fsi->second.minX);
		target.draw(fsi->second.maxX);
	}

	if (craneVisible) target.draw(crane);

	if (tracksVisible) {
		// sf::FloatRect view = target.getView().getViewport();
		recti rect = game->metroStation->getRect();

		int minx = (int)floor(dmin.x / 32);
		int maxx = 0;
		Sprite t = track;
		if (minx < floor(rect.minX() / 4.0f)) {
			// Render tracks on the left
			maxx = (int) ceil(rect.minX() / 4.0f);
			for (int x = minx; x < maxx; x++) {
				int offl = std::max<int>(0, (int)(dmin.x - x*32));
				int offr = std::max<int>(0, (x+1)*4 - rect.minX()) * 8;
				sf::IntRect sr = track.getTextureRect();
				sr.position.x += offl;
				sr.size.x -= offl+offr;
				t.setTextureRect(sr);

				t.setPosition({x * 32.0f + offl, t.getPosition().y});
				target.draw(t);

				game->drawnSprites++;
			}
		}

		minx = (int)floor(rect.maxX() / 4.0f);
		if (minx < floor(dmax.x/ 32)) {
			// Render tracks on the right
			maxx = (int)ceil(dmax.x / 32);
			for (int x = minx; x < maxx; x++) {
				int offl = std::max<int>(0, rect.maxX() - x*4) * 8;
				int offr = std::max<int>(0, (int)((x+1)*32 - dmax.x));
				sf::IntRect sr = track.getTextureRect();
				sr.position.x += offl;
				sr.size.x -= offl+offr;
				t.setTextureRect(sr);

				t.setPosition({x * 32.0f + offl, t.getPosition().y});
				target.draw(t);

				game->drawnSprites++;
			}
		}
	}
}
