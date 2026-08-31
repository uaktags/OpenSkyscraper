#pragma once

#include "GameObject.h"
#include "Sprite.h"
#include "Math/Vector2D.h"

#include <SFML/Graphics/Drawable.hpp>
#include <map>
#include <set>
#include <string>

namespace tinyxml2 { class XMLPrinter; class XMLElement; }

namespace OT
{
	/** Random emergency events: fire and terror (bomb threat).
	 *
	 * Mirrors EventT.h/c, FireT.h/c and the emergency portions of Emergen.h/c
	 * from the original SimTower source (codemap.md:749-773, 833-862, 958-990).
	 *
	 * Events are rare occurrences that require player intervention:
	 *
	 *  - Fire: ignites at a random occupied tenant. Spreads to adjacent
	 *    tiles over time, stressing and eventually destroying tenants.
	 *    The player calls a helicopter (click the fire) which arrives after
	 *    a delay and extinguishes the blaze. Costs $ to dispatch.
	 *
	 *  - Terror (bomb): a bomb appears at a random floor location with a
	 *    countdown timer. The player must click the bomb to defuse it.
	 *    If the timer expires the bomb detonates, damaging nearby tenants
	 *    and inflicting a funds penalty + heavy stress.
	 *
	 * Only one event is active at a time. Events only trigger once the
	 * tower reaches 2 stars.
	 *
	 * Scheduling ported from OpenSkyWeb ISSUE-036 (authentic triggers):
	 *  - random events (fire 50% / burglar 50%) are spaced 30-90 in-game
	 *    DAYS apart (the old code passed days through hourToAbsolute, which
	 *    has no branch above 24h — undefined behaviour that broke cadence);
	 *  - the terrorist bomb threat is deterministic: once per game, on the
	 *    weekend day of Q4 in year 5 at 10:00, detonating at 15:00 the same
	 *    day;
	 *  - buried treasure can surface (8%/day) while something is under
	 *    construction (ISSUE-034).
	 *
	 * Art resources (loaded by SimTowerLoader, previously unused):
	 *   simtower/fire/large   (384x36, 4 animation frames)
	 *   simtower/fire/small   (384x36, 4 frames)
	 *   simtower/fire/chopper (96x36)
	 *   simtower/fire/destroyed (248x24)
	 *   simtower/alerts/fire, simtower/alerts/terrorist, simtower/alerts/chopper,
	 *   simtower/alerts/treasure
	 */
	class EventSystem : public GameObject, public sf::Drawable
	{
	public:
		enum class Type { None, Fire, Terror, Treasure };

		EventSystem(Game * game);

		/// Per-frame hook. Handles event scheduling and active-event
		/// progression (fire spread, bomb countdown, helicopter arrival).
		void advance(double dt);

		/// Reset all state. Called from Game::clearWorld().
		void reset();

		/// Handle a click at the given tile coordinate. Returns true if the
		/// click was consumed (i.e. it hit the active event). For fire this
		/// dispatches the helicopter; for terror this defuses the bomb.
		bool handleClick(int2 tile);

		/// Is an event currently active?
		bool isActive() const { return type != Type::None; }
		Type getActiveType() const { return type; }

		/// Persist / restore across save-reload.
		void encodeXML(tinyxml2::XMLPrinter & xml) const;
		void decodeXML(tinyxml2::XMLElement & xml);

	protected:
		void draw(sf::RenderTarget & target, sf::RenderStates states) const;

	private:
		void scheduleNextEvent();
		void startFire();
		void startTerror();
		void startBurglar();
		void endEvent();

		/// True when the tower contains a SECOM Center (OpenSkyWeb
		/// ISSUE-031): auto-senses fires and auto-scans bomb threats.
		bool hasSecom() const;
		/// Dispatch the paid helicopter/fire crew (shared by the click
		/// path and the fire-crew choice dialog).
		void dispatchHelicopter();
		/// Player answers to the fire-crew choice dialog (true = call).
		void resolveFireResponse(bool callCrew);
		/// Player answers to the ransom choice dialog (true = pay).
		void resolveTerrorResponse(bool pay);

		/// Pick a random occupied tenant tile as the fire origin.
		int2 pickRandomTenantTile() const;
		/// Pick a random tile inside an item that is under construction.
		int2 pickRandomConstructionTile() const;
		/// True when any Security office patrols within range of the floor.
		bool securityCoversFloor(int floor) const;
		/// Daily buried-treasure roll while construction is active.
		void checkTreasure();
		void startTreasure();
		/// Apply stress to people at/near the burning tiles.
		void applyFireStress(double dt);
		/// Spread fire to a new adjacent tile.
		void spreadFire();

		Type   type;
		double nextEventTime;

		// --- Fire state ---
		// Burning tiles stored as floor -> set of x positions. Using a map
		// of sets because int2 has no operator< for direct set<int2>.
		std::map<int, std::set<int>> burningTiles;
		int2    fireOrigin;
		double  fireSpreadTimer;   // counts down to next spread
		double  fireAnimTime;      // drives the flicker frame selection
		bool    helicopterCalled;
		double  helicopterArriveTime;
		// Tiles that have burned down (rendered as ruins).
		std::set<long> destroyedTiles; // encoded as floor*100000 + x

		// --- Terror state ---
		int2   bombLocation;
		double bombDetonateTime;
		bool   defused;
		// Deterministic threat bookkeeping (ISSUE-036): the bomb threat is a
		// once-per-game scripted event; pendingTerror defers it while another
		// event is active.
		bool   terrorFired;
		bool   pendingTerror;

		// --- Buried treasure state (ISSUE-034) ---
		int2   treasureOrigin;
		double treasureEndTime;
		int    lastTreasureDay;

		// --- Response flow state (OpenSkyWeb ISSUE-033/036) ---
		bool   brigadeDeclined;    ///< player declined the paid fire crew
		double securityArriveTime; ///< security quench completion time
		double burnoutTime;        ///< unanswered fire dies out here
		double secomScanTime;      ///< SECOM bomb-scan reveal time
		double secomDispatchTime;  ///< SECOM auto-defusal completion time
		int    ransom;             ///< terrorist ransom demand ($10k-$90k)
		bool   securityCovered;    ///< security covers the bomb floor
		int    defuseRadius;       ///< click radius (6, or 9 with security)

		// --- Sprites ---
		Sprite fireLargeSprite;
		Sprite chopperSprite;
		Sprite alertSprite;
		Sprite destroyedSprite;

		/// Convert a tile (x, floor) to world-pixel coordinates.
		static sf::Vector2f tileToPixel(int x, int floor) {
			return sf::Vector2f(x * 8.0f, -floor * 36.0f);
		}
		/// Encode a tile into a single long key for the destroyed set.
		static long encodeTile(int floor, int x) { return (long)floor * 100000L + x; }
	};
}
