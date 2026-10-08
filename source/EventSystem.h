#pragma once

#include "GameObject.h"
#include "FacilitySystem.h"
#include "PowerSystem.h"
#include "VenueSystem.h"
#include "SpecialSystem.h"
#include "YootExtrasSystem.h"
#include "PeopleSystem.h"
#include "Sprite.h"
#include "YootEvents.h"
#include "Math/Vector2D.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	namespace Item { class Item; }

	/** Scheduled disasters: fire, bomb threat and buried treasure
	 * (TODO.md "Concilia-tower adoption review (2026-09-14)" rows 12-18).
	 *
	 * Re-expressed from concilia-tower's clean-room SIMTOWER.EXE decode
	 * (EventT seg_10c8, FireT seg_10e8, TimeT seg_1208, SantaT seg_11b8,
	 * LevelT seg_1148). All constants are byte-verified tuning values; see
	 * local://ground-truth.md.
	 *
	 * Calendar (TimeT 1208:0342) — deterministic, no dice:
	 *  - fire: every 84th day (day % 84 == 83) at 10:00, gated on
	 *    star > 2 AND a security office exists AND no cathedral;
	 *  - bomb threat: every 60th day (day % 60 == 59) at 10:00, only at
	 *    stars 2/3/4, gated on a security office. The fire check runs
	 *    first (TimeT @0357 before @0373);
	 *  - Santa fly-by: every 12th day (day % 12 == 11) at 21:00.
	 *
	 * The fire model runs on EXE frames (1 day = 2400 frames of the
	 * displayed clock, hour h = frame 100h): per-floor left/right fronts
	 * advance 1 cell every 7 frames, each floor above the origin ignites
	 * on the origin cell 80 frames per floor later, never downward, and a
	 * front dies at the floor extent. Unfought fires hard-stop at 21:00.
	 * The $500,000 chopper sweeps left 1 cell/frame; foot guards (6 per
	 * security office) douse fronts with a 5-frame action. Burned cells
	 * become rubble (simtower/fire/rubble), no cash damage charge.
	 *
	 * DIVERGENCE (ORIGINAL-BUGS B7): the chopper sweeps to the actual
	 * leftmost burning front instead of the origin floor's left extent, so
	 * a wider floor's front can no longer slip past the sweep alive.
	 *
	 * The burglar event was retired (row 13): it does not exist in the EXE.
	 *
	 * YootTower (--ruleset yoottower) replaces the calendar with the world's
	 * T2EV list (YootEventScheduler; EventSystemYoot.cpp): fire, terrorist,
	 * Santa and the daily blackout check reuse the paths below with the
	 * FBEV/TREV/SAEV/BOEV parameters; Uncle Reggae and the rooftop
	 * decorations are native; the world X-events are hooks (game-parity.md
	 * V1-V7). Buried treasure is SimTower-only.
	 */
	class EventSystem : public GameObject, public sf::Drawable
	{
	public:
		/// Active event kinds. Treasure is not an active event: it pays
		/// out immediately and queues its notice.
		enum class Type { None, Fire, Terror };

		EventSystem(Game * game);

		/// Per-frame hook: the 10:00/21:00 calendar, then the active
		/// event's frame-accurate progression.
		void advance(double dt);

		/// Reset all state. Called from Game::clearWorld().
		void reset();

		/// Handle a click at the given tile coordinate. For a fire this
		/// buys the helicopter mid-blaze (the hidden 10e8:01e2 path).
		bool handleClick(int2 tile);

		/// Is an event currently active?
		bool isActive() const { return type != Type::None; }
		Type getActiveType() const { return type; }

		/// Persist / restore across save-reload.
		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

		/// Promotion-ceremony hook (LevelT @0047-004a): re-seed the buried
		/// treasure latch with the NEW tier's dig condition, so a basement
		/// over-dug before the promotion forfeits that tier's treasure and
		/// anything less re-arms it. Called by LevelUp/Game::ratingMayIncrease
		/// once per promotion with the new star rating. Safe at star > 3 or
		/// with no basement (sets the latch to false).
		void armTreasureLatch(int star);
		/// Hidden debug menu triggers (EXE actions 9002/9000/9001). They
		/// run the calendar's own scheduling paths (startFire / offerBomb
		/// / payTreasure), so the paths' own gating still applies; only
		/// the calendar scheduling is skipped. Reachable only via the
		/// --debug-menu dev menu in Game.
		void debugTriggerFire();
		void debugTriggerTerror();
		void debugTriggerTreasure();

		/// YootTower power and blackout (BOEV). Inert in other rulesets.
		PowerSystem power;
		/// YootTower trash, restrooms and doctors (plan step 12). Inert elsewhere.
		FacilitySystem facilities;
		/// YootTower venues, final items and the subway (plan step 14 B). Inert elsewhere.
		VenueSystem venues;
		/// YootTower front desk, housekeeping, parking, fire dept (step 14).
		SpecialSystem specials;
		/// YootTower school, kite/dolphins, Mr. Maru, hot spring, traffic map.
		YootExtrasSystem extras;
		/// Layer-1 world sprites (the Kegon kite), drawn before the tower.
		void drawBehindTower(sf::RenderTarget & target) const { if (yoot) extras.drawBehindTower(target); }
		void drawUnderMovers(sf::RenderTarget & target) const { if (yoot) venues.drawTrains(target); }
		/// YootTower visitor pool and walking visitors (P4). Inert elsewhere.
		PeopleSystem people;
		/// True while the YootTower T2EV scheduler drives the calendar.
		bool yootMode() const { return yoot; }
		const YootEventScheduler & yootScheduler() const { return scheduler; }

	protected:
		void draw(sf::RenderTarget & target, sf::RenderStates states) const;

	private:
		struct Guard;
		struct GuardOffice;

		// --- calendar ---
		void dailyDispatch();
		void launchSanta();

		// --- YootTower T2EV scheduler (EventSystemYoot.cpp) ---
		void syncRuleset();
		void yootDispatch();
		void yootBegin(int index);
		void yootSyncRunning();
		void yootAdvanceSanta(double frames);
		void startReggae(int index);
		void stepReggae(int minute);
		void endReggae(bool caught);
		void drawOutDecorations(sf::RenderTarget & target) const;
		sf::Texture * outDecorationTexture(int imageId) const;

		// --- fire (FireT seg_10e8) ---
		void startFire();
		void offerFireChopper();
		void resolveFireResponse(bool callCrew);
		void callChopper();
		void stepFireFrame();
		void stepFireGuards();
		void deployFireGuards();
		void destroyFireCell(int floor, int x);
		void endFire();
		bool fireBurning() const;

		// --- bomb (EventT seg_10c8) ---
		void offerBomb();
		void resolveBombResponse(bool pay);
		void armBomb();
		void stepBombGuards();
		void deployBombGuards();
		void detonateBomb();

		// --- buried treasure (LevelT seg_1148) ---
		void notifyBuilt();
		bool treasureCondition(int star) const;
		void payTreasure(int star);

		// --- shared helpers ---
		void addRubble(int floor, int x);
		bool floorExtent(int floor, int & left, int & right) const;
	int  topBuiltFloor() const;
		int  lobbyHeight() const;
		int  pickDisasterFloor() const;
		bool hasSecurity() const;
		double elapsedFrames() const;
		/// Current EXE clock frame (hour h = frame 100h; 1 day = 2400).
		int  clockFrame() const;
		/// Next unsearched floor for a guard's frontier, or INT_MIN when
		/// the search is exhausted (GuardT 10f8:0701).
		int  nextGuardFloor(GuardOffice & office, const Guard & g) const;

		Type   type;

		// --- ruleset parameters (SimTower defaults; YootTower from T2EV) ---
		bool   yoot;
		std::string yootKey;     ///< world id the scheduler was built for
		YootEventScheduler scheduler;
		int    chopperCost;      ///< dollars
		int    fireEndFrame;     ///< unfought fires stop here
		int    bombDetonateFrame;
		int    yootFireIdx, yootTerrorIdx, yootBlackoutIdx, yootSantaIdx;
		int    santaStartMinute; ///< YootTower: minutes flown since launch
		double santaMinutes;

		// --- Uncle Reggae (T2Maru_Reggae) ---
		struct Reggae {
			bool active = false;
			int  index = -1;
			Item::Item * target = nullptr;
			int  counter = 0;      ///< +0x54: one tick per 3 minutes
			int  nextMinute = 0;   ///< +0x50
			int  guardMinute = -1; ///< security arrival (approximation)
			YootReggaeDef def;
			YootVisitor * walker = nullptr; ///< the pool VIP who walks (PeopleSystem)
		} reggae;
		mutable std::map<int, sf::Texture> outDecoTextures;

		// --- calendar state ---
		double prevHour;        ///< displayed hour of the previous advance
		int    lastDispatchDay; ///< day of the last 10:00 dispatch (idempotence)
		bool   santaActive;
		float  santaX;
		float  santaY;

		// --- fire state ---
		std::map<int, int> fireLeft;  ///< floor -> left front cell (absent = not burning)
		std::map<int, int> fireRight; ///< floor -> right front cell
		int    fireOriginFloor;
		int    fireOriginCell;
		int    fireFrame;       ///< EXE frames since ignition
		double fireFrameAccum;  ///< sub-frame carry
		bool   fireChopperPending; ///< the ignition dialog is on screen
		bool   fireChopperPaid;    ///< the helicopter has been bought
		int    chopperX;        ///< > 0 while the chopper sweeps left
		bool   fireGuardsActive;

		// --- guards (shared by the fire and bomb responses) ---
		struct Guard {
			int  floor;
			int  x;
			int  transit;   ///< invisible frames per floor
			int  pause;     ///< action pause frames remaining
			bool below;     ///< expand downward (guards 3-5)
			bool douse;     ///< a douse lands when the pause ends (fire mode)
			bool retired;
		};
		struct GuardOffice {
			int   floor;
			int   up;    ///< highest claimed floor
			int   down;  ///< lowest claimed floor
			Guard g[6];  ///< kGuardsPerOffice
		};
		std::vector<GuardOffice> guardOffices;

		// --- bomb state ---
		int    bombFloor;
		int    bombSlot;
		int    bombRansom;
		bool   bombArmed;
		bool   bombCaught;

		// --- buried treasure state ---
		bool   treasureLatched; ///< true = paid/forfeited for this tier
		std::set<Item::Item *, SimLess> pendingBuilds; ///< items seen under construction

		// --- rubble ---
		std::set<long> rubble;

		// --- sprites ---
		Sprite fireSprite;
		Sprite chopperSprite;
		Sprite alertSprite;
		Sprite rubbleSprite;
		Sprite santaSprite;

		/// Convert a tile (x, floor) to world-pixel coordinates.
		static sf::Vector2f tileToPixel(int x, int floor) {
			return sf::Vector2f(x * 8.0f, -floor * 36.0f);
		}
		/// Encode a tile into a single non-negative long key for the rubble
		/// set (basements are negative floors, and blasts reach negative x).
		static long encodeTile(int floor, int x) {
			return (long)(floor + 1000) * 100000L + (x + 1000);
		}
		static void decodeTile(long key, int & floor, int & x) {
			floor = (int)(key / 100000L) - 1000;
			x = (int)(key % 100000L) - 1000;
		}
	};
}
