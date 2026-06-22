#pragma once

#include <tinyxml2.h>
#include <SFML/Audio/Sound.hpp>
#include <map>

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
#include "JudgeSystem.h"
#include "LevelUp.h"
#include "VipSystem.h"

namespace OT {
	namespace Item { class AbstractPrototype; }

	class Game : public State
	{
	public:
		Application & app;
		Game(Application & app);
		virtual ~Game();

		typedef std::set<Sprite *> Sprites;
		Sprites sprites;

		unsigned int drawnSprites;

		void activate();
		void deactivate();

		bool handleEvent(sf::Event & event);
		void advance(double dt);
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
		void extendFloor(int floor, int minX, int maxX);

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

	ToolboxWindow toolboxWindow;
		TimeWindow    timeWindow;
		InspectorDialog inspectorDialog;
		ElevatorDialog  elevatorDialog;
		LevelUpDialog   levelUpDialog;
		MapWindow     mapWindow;
		FinanceWindow financeWindow;

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

		/// Toggle elevator service for a floor. Encapsulates the gameMap +
		/// cleanQueues + updateRoutes dance so UI dialogs don't need to reach
		/// into the pathfinder directly.
		void toggleElevatorService(class Item::Elevator::Elevator * e, int floor);

		Route visualizeRoute;

		GameMap gameMap;
		PathFinder pathFinder;
		Decorations decorations;

		/// Daily tenant & hotel evaluation engine. Updated once per day from
		/// settleDailyAccounting(); scores are cached on each Item::evaluation.
		JudgeSystem judgeSystem;

		/// Periodic VIP visit system (Phase 3.2). Schedules random VIP
		/// visits once the tower reaches 2 stars / 100+ population, and
		/// grants bonuses (or complaints) based on the latest judge pass.
		VipSystem vipSystem;

	public:
		void encodeXML(tinyxml2::XMLPrinter & xml);
		void decodeXML(tinyxml2::XMLDocument & xml);
		void reloadGUI();

	private:
		void clearWorld();
		void settleDailyAccounting();
		int calculateDailyMaintenanceCost() const;
		int lastAccountingDay;
		/// Quarter value at the last settleDailyAccounting. When the live
		/// time.quarter advances past this, we call Money::finalizeQuarter
		/// so the finance window's quarterly accumulators reset.
		int lastAccountingQuarter;
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
