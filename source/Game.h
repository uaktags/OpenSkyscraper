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
#include "Sky.h"
#include "Sound.h"
#include "Sprite.h"
#include "State.h"
#include "Time.h"
#include "Decorations.h"
#include "ToolboxWindow.h"
#include "TimeWindow.h"

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

		Item::Factory itemFactory;

		typedef std::set<Item::Item *> ItemSet;
		ItemSet items;
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
		int rating;
		int population;
		bool populationNeedsUpdate;
		void transferFunds(int f, std::string message = std::string());
		void setFunds(int f);
		void setRating(int r);
		void setPopulation(int p);
		void ratingMayIncrease();

		ToolboxWindow toolboxWindow;
		TimeWindow    timeWindow;

		Time time;
		int speedMode;
		void setSpeedMode(int sm);

		std::string selectedTool;
		int2 toolPosition;
		Item::AbstractPrototype * toolPrototype;
		Item::Item * itemBelowCursor;
		void selectTool(const char * tool);

		Sky sky;

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

		Route visualizeRoute;

		GameMap gameMap;
		PathFinder pathFinder;
		Decorations decorations;

	private:
		double zoom;
		double2 poi;

		//Rocket::Core::ElementDocument * mapWindow;

		void reloadGUI();

		void encodeXML(tinyxml2::XMLPrinter & xml);
		void decodeXML(tinyxml2::XMLDocument & xml);
	};
}
