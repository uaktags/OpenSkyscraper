/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Cathedral — wedding church / "Tower of the Year" endgame venue.
 * Originally ported from OpenSkyWeb (src/game/items/cathedral.js,
 * ISSUE-010); reworked 2026-09-14 to the EXE's ChurchT script (concilia-
 * tower decode, TODO.md row 23): no instant TOWER on completion — the
 * cathedral hosts the 5-star -> TOWER wedding instead. */
#include "../Game.h"
#include "../Application.h"
#include "Cathedral.h"

using namespace OT;
using namespace OT::Item;

// --- ChurchT wedding constants (concilia-tower decode of seg_1040) ---
static const int    kWeddingGuestCount   = 40;    // OpenChurch summons 40 (8 guests x 5 sections)
static const double kWeddingSummonHour   = 7.0;   // OpenChurch 7:00 AM, weekend travel only
static const double kWeddingDeadlineHour = 12.5;  // InChurchPeple deadline ft 0x320 = 12:30 PM
static const double kWeddingCloseHour    = 13.0;  // CloseChurch 1:00 PM dismissal
static const int    kWeddingPopulation   = 15000; // CheckMarry star-5 population line (tuning +0x50)
// Weddings run on the weekend/holiday (Time::isHoliday; SimTower day == 2).
static const int    kWeddingStarRating   = 4;     // internal rating 4 = 5 stars
static const int    kCoupleSheetCells    = 8;     // 0x8828 sheet: walk-in 0/1, ceremony 2..6, held pose 7
static const int    kCoupleWalkinSteps   = 14;    // counter < 14: the two walk in (bride cell 0, groom cell 27)
static const int    kCoupleGroomCell     = 27;    // the groom's start cell (28-cell ceremony art span)
static const int    kCoupleMeetCell      = 13;    // they meet at cell 13 and hold the ceremony there
static const double kCoupleStepFrames    = 20.0;  // AnimateWeddingCouple: one script step per 20 frames
static const int    kCoupleCounterMax    = 1000;  // held pose forever (EXE caps the counter)
static const double kSimFramesPerDay     = Time::kSimFramesPerDay; // EXE: 2600 frames/day

Cathedral::~Cathedral()
{
	clearGuests();
}

void Cathedral::init()
{
	Item::init();
	weddingPhase = kWeddingIdle;
	weddingDone = false;
	weddingCounter = 0;
	weddingFrameAccumulator = 0;
	lastAdvancedAt = game->time.absolute;
	coupleCellWidth = 16;
	coupleArtReady = false;

	// The SIMTOWER.EXE resources carry no dedicated cathedral bitmap; the
	// web port composes "simtower/cathedral/main" from the party hall art
	// (resource 0x8b28). Draw one party hall frame anchored bottom-left.
	sprite.SetImage(App->bitmaps.editionTexture("partyhall", "simtower/partyhall"));
	sprite.setTextureRect(sf::IntRect({0, 0}, {192, 60}));
	sprite.setOrigin({0.f, 60.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	addSprite(&sprite);
}

void Cathedral::advance(double dt)
{
	Item::advance(dt);
	if (underConstruction) return;

	// OpenChurch, 7:00 AM (weekend guests only). Yesterday's dismissed
	// guests have had the night to walk home; drop their objects first
	// (the Cinema clearCustomers-at-next-open idiom).
	if (game->time.checkHour(kWeddingSummonHour)) {
		clearGuests();

		// CheckMarry eligibility: 5 stars, the 15,000-population line, a
		// completed cathedral, and no wedding held yet.
		if (weddingPhase == kWeddingIdle && !weddingDone &&
		    game->rating == kWeddingStarRating &&
		    game->population >= kWeddingPopulation &&
		    game->time.isHoliday())
		{
			weddingPhase = kWeddingGuestsArriving;
			for (int i = 0; i < kWeddingGuestCount; i++) {
				Guest * p = new Guest(this);
				guests.insert(p);
				// Travel in from the lobby like any venue crowd; queues
				// and car capacity do the morning trickle.
				p->journey.set(lobbyRoute);
			}
			game->timeWindow.showMessage("Wedding guests are on their way to the Cathedral");
			LOG(IMPORTANT, "wedding summoned %d guests (day %d)", kWeddingGuestCount, game->time.day);
		}
	}

	// InChurchPeple: the ceremony fires the moment the LAST of the 40
	// guests walks in — but only before the 12:30 deadline. A short
	// congregation means no wedding today.
	if (weddingPhase == kWeddingGuestsArriving &&
	    static_cast<int>(people.size()) >= kWeddingGuestCount &&
	    game->time.hour < kWeddingDeadlineHour)
	{
		startWedding();
	}

	// CloseChurch, 1:00 PM: the event ends whether or not the ceremony
	// fired; everyone goes home.
	if (weddingPhase != kWeddingIdle && game->time.checkHour(kWeddingCloseHour))
		endWeddingEvent();

	// AnimateWeddingCouple: one script step every 20 frames while the
	// ceremony runs (counter = the EXE event dword 0xB40C).
	if (weddingPhase == kWeddingCeremony) {
		double frames = (game->time.absolute - lastAdvancedAt) * kSimFramesPerDay;
		if (frames > 0)
			weddingFrameAccumulator += frames;
		while (weddingFrameAccumulator >= kCoupleStepFrames &&
		       weddingCounter < kCoupleCounterMax) {
			weddingCounter++;
			weddingFrameAccumulator -= kCoupleStepFrames;
		}
		if (frames > 0 && coupleArtReady)
			updateCoupleSprites();
	}
	lastAdvancedAt = game->time.absolute;
}

/** StartMarry (1040:02B5): the wedding begins the moment the 40th guest
 * is inside — bells, couple animation, camera pan to the church, and the
 * TOWER promotion evaluated right then (the EXE sets star_rating 6
 * mid-ceremony; here LevelUp's cathedral+wedding row does the crown via
 * ratingMayIncrease). */
void Cathedral::startWedding()
{
	weddingPhase = kWeddingCeremony;
	weddingCounter = 0;
	weddingFrameAccumulator = 0;
	weddingDone = true;

	game->centerViewportOnTile(position.x + size.x / 2.0, position.y + size.y / 2.0);
	game->playOnce("simtower/bells");

	// The cathedral wears the ceremony art: six 224x36 strips (resource
	// ids 0x8CE9..0x8DEA), key 0 the topmost cherub/"Welcome to Tower"
	// banner floating above the dome, bottom-anchored at the church's
	// lower-left cell so the stack rises one floor taller than the
	// building. Only draw a strip when the sheet loaded at strip size —
	// BitmapManager placeholders (2048x512) must not cover the tower.
	for (int k = 0; k < 6; k++) {
		char key[32];
		snprintf(key, sizeof(key), "simtower/church/ceremony%d", k);
		sf::Texture & tex = App->bitmaps[key];
		if (tex.getSize().x > 448 || tex.getSize().y != 36) continue;
		ceremonyStrips[k].setTexture(tex, true);
		ceremonyStrips[k].setOrigin({0.f, 36.f});
		ceremonyStrips[k].setPosition({static_cast<float>(position.x * 8),
		                               static_cast<float>(-(position.y + (5 - k)) * 36)});
		addSprite(&ceremonyStrips[k]);
	}

	// The couple (0x8828, 128x36 = eight 16px cells): bride frame 0 and
	// groom frame 1 walk in, ceremony frames 2..6, held pose 7.
	sf::Texture & couple = App->bitmaps["simtower/church/couple"];
	coupleArtReady = false;
	if (couple.getSize().x <= 256 && couple.getSize().y == 36) {
		coupleCellWidth = couple.getSize().x / kCoupleSheetCells;
		coupleArtReady = true;
		coupleA.setTexture(couple, true);
		coupleA.setOrigin({0.f, 36.f});
		coupleB.setTexture(couple, true);
		coupleB.setOrigin({0.f, 36.f});
		updateCoupleSprites();
	}

	// Mid-ceremony promotion point: LevelUp's 5-star -> TOWER row sees
	// weddingCompleted() flip true and crowns right now.
	game->ratingMayIncrease();
	LOG(IMPORTANT, "wedding ceremony started: %d guests inside, TOWER evaluated", kWeddingGuestCount);
}

/** CloseChurch (1040:0179): the ceremony (if any) ends and every guest
 * of the venue — inside or still inbound — turns for home. */
void Cathedral::endWeddingEvent()
{
	weddingPhase = kWeddingIdle;
	weddingCounter = 0;

	const Route & r = game->findRoute(this, game->mainLobby);
	for (Guests::iterator g = guests.begin(); g != guests.end(); g++) {
		Guest * p = *g;
		if (r.empty()) {
			LOG(DEBUG, "%s guest %p has no route to leave", prototype->id.c_str(), (void *)p);
			continue;
		}
		p->state = Person::kReturning;
		p->from = prototype->name;
		p->goingTo = "Exit";
		if (p->at == this)
			removePerson(p);
		p->journey.set(r);
	}

	for (int k = 0; k < 6; k++)
		removeSprite(&ceremonyStrips[k]);
	removeSprite(&coupleA);
	removeSprite(&coupleB);
}

/** Applies weddingCounter to the couple sprites (AnimateWeddingCouple
 * script, 1028:17F0): counter < 14 the bride (frame 0) walks in from
 * cell 0 while the groom (frame 1) walks from cell 27; then ceremony
 * frames 2..6 at the meeting cell, and the held pose 7 until
 * CloseChurch. */
void Cathedral::updateCoupleSprites()
{
	const float baseX = static_cast<float>(position.x * 8);
	const float baseY = static_cast<float>(-position.y * 36);
	const int cw = static_cast<int>(coupleCellWidth);

	if (weddingCounter < kCoupleWalkinSteps) {
		coupleA.setTextureRect(sf::IntRect({0, 0}, {cw, 36}));
		coupleA.setPosition({baseX + weddingCounter * 8.f, baseY});
		addSprite(&coupleA);
		coupleB.setTextureRect(sf::IntRect({cw, 0}, {cw, 36}));
		coupleB.setPosition({baseX + (kCoupleGroomCell - weddingCounter) * 8.f, baseY});
		addSprite(&coupleB);
	} else {
		int f = weddingCounter < kCoupleWalkinSteps + 5 ? weddingCounter - 12 : 7;
		coupleA.setTextureRect(sf::IntRect({f * cw, 0}, {cw, 36}));
		coupleA.setPosition({baseX + kCoupleMeetCell * 8.f, baseY});
		addSprite(&coupleA);
		removeSprite(&coupleB); // idempotent (set erase)
	}
}

bool Cathedral::towerHasCathedral(Game * game)
{
	const Game::ItemSet & cathedrals = game->itemsByType["cathedral"];
	for (Game::ItemSet::const_iterator i = cathedrals.begin(); i != cathedrals.end(); i++)
		if (!(*i)->underConstruction)
			return true;
	return false;
}

bool Cathedral::weddingCompleted(Game * game)
{
	const Game::ItemSet & cathedrals = game->itemsByType["cathedral"];
	for (Game::ItemSet::const_iterator i = cathedrals.begin(); i != cathedrals.end(); i++) {
		Cathedral * c = dynamic_cast<Cathedral *>(*i);
		if (c && c->weddingDone)
			return true;
	}
	return false;
}

Cathedral * Cathedral::weddingVenue(Game * game)
{
	const Game::ItemSet & cathedrals = game->itemsByType["cathedral"];
	for (Game::ItemSet::const_iterator i = cathedrals.begin(); i != cathedrals.end(); i++) {
		Cathedral * c = dynamic_cast<Cathedral *>(*i);
		if (c && c->weddingPhase != kWeddingIdle)
			return c;
	}
	return NULL;
}

Cathedral::Guest::Guest(Cathedral * church)
:	Person(church->game)
{
	Type types[] = {kMan, kWoman1, kWoman2, kWomanWithChild1};
	type = types[rand() % 4];
	from = "City";
	goingTo = church->prototype->name;
}

void Cathedral::clearGuests()
{
	for (Guests::iterator g = guests.begin(); g != guests.end(); g++)
		delete *g;
	guests.clear();
}

void Cathedral::encodeXML(tinyxml2::XMLPrinter & xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("weddingDone", (int)weddingDone);
}

void Cathedral::decodeXML(tinyxml2::XMLElement & xml)
{
	Item::decodeXML(xml);
	// "ceremony" is the pre-rework flag (bells + instant TOWER on
	// completion); a tower that already held it counts as wed.
	weddingDone = xml.BoolAttribute("weddingDone", xml.BoolAttribute("ceremony", false));
	// The live event state is transient: a save made mid-wedding resumes
	// with the crown latched (weddingDone) but no ceremony replay.
	weddingPhase = kWeddingIdle;
	weddingCounter = 0;
}

void Cathedral::evacuateForDisaster()
{
 clearGuests(); weddingPhase = kWeddingIdle; weddingCounter = 0;
 weddingFrameAccumulator = 0; lastAdvancedAt = game->time.absolute;
 updateCoupleSprites();
}
