#pragma once

#include <TGUI/Widgets/ChildWindow.hpp>
#include <functional>
#include <tinyxml2.h>
#include <SFML/Audio/Sound.hpp>
#include <map>
#include <set>
#include <vector>

#include "Item/Elevator/Elevator.h"
#include "Item/Factory.h"
#include "Item/Floor.h"
#include "Item/Item.h"
#include "PathFinder/GameMap.h"
#include "PathFinder/PathFinder.h"
#include "Money.h"
#include "Lighting.h"
#include "Sky.h"
#include "Sound.h"
#include "Sprite.h"
#include "State.h"
#include "Time.h"
#include "Decorations.h"
#include "ToolboxWindow.h"
#include "TimeWindow.h"
#include "InspectorDialog.h"
#include "ElevatorDialog.h"
#include "LevelUpDialog.h"
#include "MapWindow.h"
#include "FinanceWindow.h"
#include "ReportDialog.h"
#include "JudgeSystem.h"
#include "LevelUp.h"
#include "VipSystem.h"
#include "EventSystem.h"
#include "BuildRules.h"
#include "Skin.h"
#include "YootCatalog.h"
#include "YootWorldArt.h"
#include <memory>


namespace OT {
	namespace Item { class AbstractPrototype; }
	namespace Item { class Item; }

	class Game : public State
	{
	public:
		Application & app;
		Game(Application & app);
		BuildRules activeRules;
		/// Switch the active ruleset/profile and apply its process-wide
		/// clock mode (YootTower minute-linear time) and calendar.
		void setRules(const BuildRules & rules);

		/// YootTower item catalog (plan step 4): prototype id -> TNDF/MVDF
		/// stats for the active world. Empty outside YootTower mode.
		std::map<std::string, YootItemStats> yootCatalog;
		/// OpenSky price/carCost of every overlaid prototype, restored when
		/// the catalog is removed.
		std::map<std::string, std::pair<int, int> > yootBasePrices;
		/// Overlay (or, outside YootTower, remove) the catalog onto the
		/// prototypes: price, carCost and prototype->yoot.
		void applyItemCatalog();
		/// Catalog entry for an item's prototype, or null.
		const YootItemStats * yootStats(const Item::Item * item) const;


		virtual ~Game();

		typedef std::set<Sprite *> Sprites;
		Sprites sprites;

		unsigned int drawnSprites;

		void activate();
		void deactivate();

		bool handleEvent(sf::Event & event);
		void advance(double dt);
		/// Headless sim step (--sim, SimHarness): the sim ticks of
		/// advance() plus the weather roll, with no rendering or input.
		/// A pending event choice is answered "yes" deterministically.
		void advanceHeadless(double dt);
		/// Event log for the headless summary; recording is off in play.
		bool simRecording = false;
		std::vector<std::string> simEvents;
		void recordSimEvent(const std::string & message);

		/// Jump the game clock to `h` (0..24) of the current day. The clock's
		/// absolute-to-hour mapping is non-linear, so this goes through
		/// Time's table. Used by the event system (e.g. a caught fire) to
		/// snap the clock forward.
		void jumpToHour(double h);
		bool handleViewportScrollbarEvent(sf::Event & event);
		void drawViewportScrollbars(sf::RenderTarget & target) const;
		void qaSetViewportScrollFractions(double horizontal, double vertical);

		Item::Factory itemFactory;

		typedef std::set<Item::Item *, SimLess> ItemSet;
		ItemSet items;
		typedef std::set<class Person *, SimLess> PersonSet;
		PersonSet people;
		typedef std::map<int, ItemSet> ItemSetByInt;
		typedef std::map<std::string, ItemSet> ItemSetByString;
		typedef std::map<int, Item::Floor *> FloorItems;
		ItemSetByInt itemsByFloor;
		ItemSetByString itemsByType;
		FloorItems floorItems;
		Item::Item * mainLobby;
		Item::Item * metroStation;
		void addItem(Item::Item * item);
		void removeItem(Item::Item * item);
        void destroyItemByDisaster(Item::Item *item);
        void evacuateForDisaster();
        void cancelTransientVip();
        bool rejectDisasterWorldEdit();
		bool extendFloor(int floor, int minX, int maxX, bool free = false);
		/// True on the days the bomb (day%60==59) or fire (day%84==83) event
		/// is offered; venues pay no income then (ASM 1180:12e7).
		bool isDisasterOfferDay() const;
		/// Mint free stub floor slabs where a loaded tower lost them
		/// (OpenSkyWeb ISSUE-041 self-heal). Returns true when repaired.
		bool repairFloorCoverage();
		/// Serialize the tower (with integrity hash) to the given path.
		bool writeTowerFile(const std::string & path);

		int64_t funds;
		Money money;
		int rating;
		int population;
		bool populationNeedsUpdate;
		void transferFunds(int64_t f, std::string category = "misc", std::string message = std::string());
		void setFunds(int64_t f);
		void setRating(int r);
		void setPopulation(int p);
		void ratingMayIncrease();

		/// EXE CountT (batch 5 T14): ten category totals plus the global.
		/// `population` mirrors countT.population.
		TenantRules::CountT countT;
		void addPopulation(int category, int delta);
		void resetCategory(int category);
		/// resetCategory at most once per simulation day: every shop / fast food /
		/// restaurant / cinema / party hall calls this when it opens, the first
		/// call of the day clears the category and later calls only add.
		void resetCategoryOnce(int category);
		/// Recompute countT once from item state (old saves without it).
		void recomputeCountT();
		/// Items on `floor` in slot order (ascending x, ties by id); floor slabs excluded.
		std::vector<Item::Item *> slotOrder(int floor) const;
		/// Count of non-infrastructure items strictly left of `item` on its floor.
		int tenantSlotIndex(const Item::Item * item) const;

		// batch 5 agent B state/APIs (declared here, defined in Game.cpp by B).
		bool medicalAdequate = true;
		bool recyclingAdequate = true;
		int  recyclingDisplayStage = 0;
		bool emergencyActive() const;
		void rebuildVenueLists();
		Item::Item *pickVenue(int originFloor, int kind);
		Item::Item *pickMedicalCenter(int originFloor);
		std::vector<Item::Item *> venueLists[7][4];

		/// YootTower VIP-visited bits (T2TowerDoc +0xa0). Each grade row's
		/// vipFlag must be set before population can promote.
		uint32_t yootVipFlags;
		/// YootTower doc+0x9c: bit 0 = secret plugins loaded (the Kegon hot
		/// spring unlocks the Spa). Saved as yootSecretItems.
		uint32_t yootSecretItems = 0;
		/// YootTower final-item hook (T2.exe FUN_00422670, doc vtable
		/// +0x188): at the top grade (grade == numGrades) OR that grade's
		/// flag into yootVipFlags so the last promotion can land. Called by
		/// the world's final event (Tokyo stadium baseball, Hawaii cathedral
		/// wedding, Kegon attraction). Returns false (no-op) otherwise.
		bool grantFinalGrade();

		// State read by ToolboxWindow during construction (it calls updateTool()/
	// updateSpeed() inside its constructor, which read these fields). They
	// must be default-constructed BEFORE the ToolboxWindow constructor runs,
	// so they are declared above toolboxWindow below. Reading them while
	// still raw uninitialised memory is UB: copying `selectedTool` allocates
	// a buffer of the source length, which is garbage → std::length_error.
	// The latent bug surfaced when ToolboxWindow::updateTool() started
	// copying the string instead of merely comparing against it.
	std::string selectedTool = "inspector";
	int speedMode = 1;
	void setSpeedMode(int sm);

	// Status overlay mode cycled with 'O'. Mirrors StatusMode.h/c from the
	// Yoot source. Read by ToolboxWindow's "View" button click handler (not
	// at construction time), but declared here for proximity to the related
	// state.
	// kOutside: Skin::Yoot only, the Control palette's "Outside" view
	// (T2WorldDef::ClickOutViewButton 0x10071c00, view mode 1).
	enum StatusMode { kNormal, kEval, kPric, kHotel, kOutside };
	StatusMode statusMode;
	void cycleStatusMode();

		/// Active art set (Skin selector). Independent of the ruleset;
		/// cycled live with K or the toolbox "Skin" button.
		Skin skin = Skin::OpenSky;
		void setSkin(Skin newSkin);
		void cycleSkin();
		/// YootTower plugin art for the active world, or null when the skin
		/// is not Yoot or the world plugin is missing (OpenSky art is used).
		const YootWorldArt * yootSkinArt() const;
		/// (Re)loads the YootTower art for the active skin and world.
		void refreshYootArt();

		/// Open the quarterly financial report dialog (Wave U row 49).
		/// Called automatically once a 4:59AM quarterly settlement
		/// commits, and bound to the 'P' key.
		void showReport();

	ToolboxWindow toolboxWindow;
		TimeWindow    timeWindow;
		InspectorDialog inspectorDialog;
		ElevatorDialog  elevatorDialog;
		LevelUpDialog   levelUpDialog;
		MapWindow     mapWindow;
		FinanceWindow financeWindow;
		ReportDialog  reportDialog;

		Time time;

		int2 toolPosition;
		Item::AbstractPrototype * toolPrototype;
		Item::Item * itemBelowCursor;
		void selectTool(const char * tool);

		Sky sky;

		/// Global time-of-day + weather tint (Phase 3.4). Refreshed each
		/// frame after the Sky state has been updated; applied per-sprite
		/// in Item::render() / Floor::render().
		Lighting lighting;

		Item::Elevator::Elevator * draggingElevator;
		int draggingElevatorStart;
		bool draggingElevatorLower;
		int draggingMotor;

		Sound cockSound;
		Sound morningSound;
		Sound bellsSound;
		Sound eveningSound;

		typedef std::set<Sound *> SoundSet;
		SoundSet autoreleaseSounds;
		SoundSet playingSounds;
		std::map<Path, double> soundPlayTimes;
		void playOnce(Path sound);
		void playRandomBackgroundSound();

		void updateRoutes();
		Route findRoute(Item::Item * start, Item::Item * destination, bool serviceRoute = false);
		void seedOfficeLunchQa();
		void seedNewTower();

		/// Centre the main viewport on the given tower tile coordinate.
		/// Used by the minimap click-to-jump.
		void centerViewportOnTile(double tileX, double tileY);

		/// Multiply the viewport zoom by `factor`, keeping the world point
		/// under `anchorPx` (window pixels, e.g. the mouse) fixed when
		/// given; otherwise the view centre stays put. Clamped to
		/// [1/3, 2]: SimTower's 3x-out .. 2x-in range (`zoom` is the
		/// fraction of the world visible, so smaller = closer).
		void zoomBy(double factor, const sf::Vector2i * anchorPx = nullptr);

		/// Modal two-button choice for emergency events (fire crew /
		/// terrorist ransom). Pauses the simulation until answered;
		/// `onChoice(true)` = the `yesLabel` button. The pending callback
		/// is dropped by clearWorld().
		void askEventChoice(const std::string & title, const std::string & text,
		                    const std::string & yesLabel, const std::string & noLabel,
		                    const std::function<void(bool)> & onChoice);
		/// Modal text entry with one OK button (YootTower T2Baseball dialog
		/// 12006). OK with an empty text keeps the dialog open (DialogHook
		/// refuses ""); headless runs accept `defaultText`.
		void askEventText(const std::string & title, const std::string & text,
		                  const std::string & defaultText,
		                  const std::function<void(const std::string &)> & onText);
		/// Close any open event choice without answering it.
		void closeEventChoice();
		/// --qa-build: runs the construction flow for prototype `id` at
		/// (x, floor) as a tool click would. Returns true when built.
		bool qaBuild(const std::string & id, int2 pos);

		/// ISSUE-039 drag-to-place batching (OpenSkyWeb port): hold an item
		/// tool and drag to lay out a whole row/column (Shift = full grid).
		/// Nothing builds or is charged until mouse release; ghosts preview
		/// per-cell validity. Esc cancels.
		struct BatchCell { int x; int y; bool valid; std::string reason; };
		bool isBatchDraggable(Item::AbstractPrototype * proto) const;
		void startBatchDrag();
		void updateBatchDrag();
		void finishBatchDrag();
		void cancelBatchDrag();
		bool batchDragActive() const { return batchDragging; }
		const std::vector<BatchCell> & getBatchCells() const { return batchCells; }
		bool batchAllAffordable() const { return batchAffordable; }

		/// Toggle elevator service for a floor. Encapsulates the gameMap +
		/// cleanQueues + updateRoutes dance so UI dialogs don't need to reach
		/// into the pathfinder directly.
		void toggleElevatorService(class Item::Elevator::Elevator * e, int floor);

		Route visualizeRoute;

		GameMap gameMap;
		PathFinder pathFinder;
		/// Items waiting for their lobby route to be recomputed (see updateRoutes).
		std::vector<Item::Item *> routeBuildQueue;
		void pumpRouteBuilds();
		Decorations decorations;
		std::unique_ptr<YootWorldArt> yootArt;
		double yootClock = 0; ///< real seconds, drives the SE (sign light) cycle

		/// Daily tenant & hotel evaluation engine. Updated once per day from
		/// settleDailyAccounting(); scores are cached on each Item::evaluation.
		JudgeSystem judgeSystem;

		/// Periodic VIP visit system (Phase 3.2). Schedules random VIP
		/// visits once the tower reaches 2 stars / 100+ population, and
		/// grants bonuses (or complaints) based on the latest judge pass.
		VipSystem vipSystem;

		/// Emergency event system (Phase 3.3). Schedules rare fire and
		/// terror (bomb) events that require player intervention. Art is
		/// loaded by SimTowerLoader (fire/large, fire/chopper, alerts/*);
		/// this system renders and drives the event lifecycle.
		EventSystem eventSystem;

	public:
		void encodeXML(tinyxml2::XMLPrinter & xml);
		void decodeXML(tinyxml2::XMLDocument & xml);
		void reloadGUI();

		/// Deregister a dying object from the pointer-keyed tracking
		/// containers (JudgeSystem visitorDaySeen/badDayStreak, EventSystem
		/// pendingBuilds). SimLess dereferences its keys during deterministic
		/// runs, so a stale entry for a freed object would be a
		/// use-after-free on the next find()/insert().
		void forgetTracking(const Person * p);
		void forgetTracking(const Item::Item * item);

		/// Grand-lobby height [bc5a]: floors 1..band-1 above the ground floor are
		/// exempt from lobby upkeep (EXE 10a8:133b, exempt iff 11 <= floor < 10 + bc5a).
		/// Total quarterly upkeep the 04:59 settlement would charge right now.
		int quarterlyUpkeepTotal() const;
		int groundLobbyBand() const { return lobbyHeightLock > 0 ? lobbyHeightLock : lobbyToolHeight(); }

	private:
		void clearWorld();
		/// New-tower funds, rating and clock from the active profile.
		void applyNewTowerProfile();
		/// YootTower promotion gate (population + VIP flag), cheap enough
		/// to run every tick (CheckGameLevel 0x10073290).
		void yootCheckGrade();
		/// Lobby height for a placement: Ctrl = 2, Ctrl+Shift = 3, otherwise
		/// the profile default (1, or the YootTower world's lobby height).
		int lobbyToolHeight() const;
		Item::Item * legacyFootprintClash(Item::Item * item);
		/// Grand-lobby height locked by the first ground-lobby build click
		/// (0 = still open). Saved with the tower.
		int lobbyHeightLock = 0;
		/// Modifier-key choice: Ctrl = 2, Ctrl+Shift = 3, otherwise the profile default.
		int lobbyHeightFromKeys() const;
		/// Tower-wide placement rules shared by click and batch paths
		/// (ceiling/depth, cathedral, above/below-ground, metro, caps,
		/// recycling adjacency, parking and ramps). Does not mutate.
		bool placementRuleBlock(Item::AbstractPrototype * proto, int x, int y, std::string & reason) const;
		/// Bulldozer rules + owner buyout; returns true when the item was removed.
		bool bulldozeItem(Item::Item * item);
		/// One simulation tick: the sim core of advance() (clock,
		/// accounting, items, people, hour boundaries). Fast mode runs
		/// several of these per rendered frame.
		void advanceSimTick(double dt);
		void settleDailyAccounting();
		int lastAccountingDay;
		int lastJudgeDay = -1000000; ///< last day the 04:59 tenant judge ran
		/// Autosave bookkeeping (OpenSkyWeb autosave.js policy): last in-game
		/// day and wall-clock second we wrote a crash-recovery snapshot.
		int lastAutosaveDay;
		double lastAutosaveWallClock;
		/// Throttled autosave, called when the in-game day rolls over.
		void autosaveIfDue();
		/// Answer + teardown for the event choice dialog.
		void answerEventChoice(bool yes);
		/// Validate a generic-room placement without mutating anything
		/// (mirror of the construction branch's checks for previews).
bool validateBatchCell(Item::AbstractPrototype * proto, int x, int y, std::string & reason, const std::set<int> & simulatedSlabs = std::set<int>()) const;
		/// Run the full construction flow for the current tool at pos.
		/// Returns true when a new item was added to the world.
		bool constructToolAt(int2 pos);
		// --- Batch drag state ---
		bool batchDragging = false;
		bool batchAffordable = true;
		int batchAnchorX = 0;
		int batchAnchorY = 0;
		std::string batchFirstReason;
		std::vector<BatchCell> batchCells;

		// --- Event choice dialog state ---
		tgui::ChildWindow::Ptr eventChoiceWindow;
		std::function<void(bool)> eventChoiceCallback;
		int eventChoiceSavedSpeed = -1;
		// --- Hidden debug menu (EXE actions 9000-9003) ---
		// The window only exists when the app was started with
		// --debug-menu; Ctrl+Shift+D toggles it.
		tgui::ChildWindow::Ptr debugMenuWindow;
		void toggleDebugMenu();
		/// EXE action 9003: force a complete repaint right now.
		void debugRedraw();
		/// Quarter value at the last settleDailyAccounting. When the live
		/// time.quarter advances past this, we call Money::finalizeQuarter
		/// so the finance window's quarterly accumulators reset.
		int lastAccountingQuarter;
		/// YootTower: financial cycle whose office rent (04:52-04:59, day 1)
		/// has been collected (plan step 7, E4).
		int yootRentCycle = -1;
	public:
		/// YootTower price selection (index into inMoney, 0 = cheapest) for
		/// an item: shops use their rent class, the rest the OpenSky default
		/// class, which maps to selection 2 (the YootTower default
		/// inMoneyIndex seen in Sample Tokyo Map.bld).
		int yootPriceSelection(const Item::Item * item) const;
		/// Pay `amount` dollars (+ income / - cost) through the YootTower
		/// ledger categories; no-op for 0.
		void yootBook(int amount, const char * category, const char * message);
	private:
		double zoom;
		double2 poi;
		/// Horizontal camera range: poi.x stays in [-limit, limit]. 512 px,
		/// or half the YootTower world's width so its edges are reachable.
		double horizontalScrollLimit() const;
		enum class ViewportDrag { None, Vertical, Horizontal };
		ViewportDrag viewportDrag;

		//Rocket::Core::ElementDocument * mapWindow;

	public:
		std::string saveFilename;
		bool isDirty;
	};
}
