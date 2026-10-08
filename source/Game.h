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

		/// Jump the game clock to `h` (0..24) of the current day. The clock's
		/// absolute-to-hour mapping is non-linear, so this goes through
		/// Time's table. Used by the event system (e.g. a caught fire) to
		/// snap the clock forward.
		void jumpToHour(double h);
		bool handleViewportScrollbarEvent(sf::Event & event);
		void drawViewportScrollbars(sf::RenderTarget & target) const;
		void qaSetViewportScrollFractions(double horizontal, double vertical);

		Item::Factory itemFactory;

		typedef std::set<Item::Item *> ItemSet;
		ItemSet items;
		typedef std::set<class Person *> PersonSet;
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
		bool extendFloor(int floor, int minX, int maxX, bool free = false);
		/// Mint free stub floor slabs where a loaded tower lost them
		/// (OpenSkyWeb ISSUE-041 self-heal). Returns true when repaired.
		bool repairFloorCoverage();
		/// Serialize the tower (with integrity hash) to the given path.
		bool writeTowerFile(const std::string & path);

		int funds;
		Money money;
		int rating;
		int population;
		bool populationNeedsUpdate;
		void transferFunds(int f, std::string category = "misc", std::string message = std::string());
		void setFunds(int f);
		void setRating(int r);
		void setPopulation(int p);
		void ratingMayIncrease();

		/// YootTower VIP-visited bits (T2TowerDoc +0xa0). Each grade row's
		/// vipFlag must be set before population can promote.
		uint32_t yootVipFlags;
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
	enum StatusMode { kNormal, kEval, kPric, kHotel };
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
		/// Close any open event choice without answering it.
		void closeEventChoice();

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
		/// One simulation tick: the sim core of advance() (clock,
		/// accounting, items, people, hour boundaries). Fast mode runs
		/// several of these per rendered frame.
		void advanceSimTick(double dt);
		void settleDailyAccounting();
		int calculateDailyMaintenanceCost() const;
		int lastAccountingDay;
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
		enum class ViewportDrag { None, Vertical, Horizontal };
		ViewportDrag viewportDrag;

		//Rocket::Core::ElementDocument * mapWindow;

	public:
		std::string saveFilename;
		bool isDirty;
	};
}
