/* Copyright (c) 2012-2015 Fabian Schuiki */
/* Copyright (c) 2026 OpenSkyscraper contributors */
#include <cmath>
#include <cassert>
#include <cstdlib>
#include <algorithm>
#include <chrono>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include "Application.h"
#include "Game.h"
#include "SaveHash.h"
#include "NameManager.h"
#include "Item/FastFood.h"
#include "Item/Lobby.h"
#include "Item/Office.h"
#include "Item/Hotel.h"
#include "SimTowerLoader.h"
#include "Item/Condo.h"
#include "Item/Elevator/Elevator.h"
#include "Item/Parking.h"
#include "Item/Housekeeping.h"
#include "Item/Shop.h"
#include "Item/Security.h"
#include "Item/Recycling.h"
#include "Item/MedicalCenter.h"
#include "Item/Metro.h"
#include "Item/Escalator.h"
#include "Item/Cathedral.h"
#include "Item/Elevator/Car.h"
#include "Item/Elevator/Queue.h"
#include "OpenGL.h"
#include "YootRules.h"
#include "YootPlacementGame.h"
#include "YootItemArt.h"
#include "YootMisc.h"
#include "YootOutView.h"
#include "YootWorld.h"
#include "SimHarness.h"

using namespace OT;
#ifdef _WIN32
#include "Math/Round.h"
#endif
// Rent resource 0x7f0b/0x3e9, expressed in dollars (the EXE stores
// hundreds). Classes are High, Average, Low and Very Low, respectively.
int openSkyQuarterlyRent(const std::string &prototypeId, int rentClass)
{
	static const int office[] = {15000, 10000, 5000, 2000};
	static const int single[] = {3000, 2000, 1500, 500};
	static const int twin[] = {4500, 3000, 2000, 800};
	static const int suite[] = {9000, 6000, 4000, 1500};
	static const int shop[] = {20000, 15000, 10000, 4000};
	static const int condo[] = {200000, 150000, 100000, 40000};
	if (rentClass < 0 || rentClass > 3) rentClass = 1;
	const int *rates = NULL;
	if (prototypeId == "office") rates = office;
	else if (prototypeId == "hotel_single" || prototypeId == "hotel") rates = single;
	else if (prototypeId == "hotel_double") rates = twin;
	else if (prototypeId == "hotel_suite") rates = suite;
	else if (prototypeId == "shop") rates = shop;
	else if (prototypeId == "condo" || prototypeId == "yoot_condo") rates = condo;
	return rates ? rates[rentClass] : 0;
}

int openSkyDefaultRentClass(const std::string &prototypeId)
{
	// EXE tenant creation (1200:1f2e) gives hotel rooms, offices and condos
	// the average class (1); the class may be changed afterwards.
	(void)prototypeId;
	return 1;
}

static int quarterlyMaintenanceCost(const Game *game)
{
	int total = 0;
	for (Game::ItemSet::const_iterator it = game->items.begin();
	     it != game->items.end(); ++it)
	{
		Item::Item *item = *it;
		if (item->underConstruction) continue;
		const std::string &id = item->prototype->id;
		if (id == "lobby")
		{
			// EXE 1180:0ae4 (all.c:20543): charged per cell unless 10a8:133b
			// (all.c:9678) says exempt, which holds only for floors 11 <=
			// floor < 10 + bc5a, i.e. the grand-lobby decks ABOVE the ground
			// floor. The ground floor itself and sky lobbies pay.
			const int y = item->position.y;
			if (!(y >= 1 && y < game->groundLobbyBand()))
			{
				if (game->rating >= 3) total += item->size.x * 1000;
				else if (game->rating >= 2) total += item->size.x * 300;
			}
		}
		else if (id == "elevator-standard" || id == "standard_elevator")
		{
			Item::Elevator::Elevator *e = dynamic_cast<Item::Elevator::Elevator *>(item);
			total += e ? (int)e->cars.size() * 10000 : 0;
		}
		else if (id == "elevator-express" || id == "express_elevator")
		{
			Item::Elevator::Elevator *e = dynamic_cast<Item::Elevator::Elevator *>(item);
			total += e ? (int)e->cars.size() * 20000 : 0;
		}
		else if (id == "elevator-service" || id == "service_elevator")
		{
			Item::Elevator::Elevator *e = dynamic_cast<Item::Elevator::Elevator *>(item);
			total += e ? (int)e->cars.size() * 10000 : 0;
		}
		else if (id == "escalator")
			total += std::max(1, item->size.y - 1) * 5000;
		else if (id == "parkingramp")
			total += Item::Parking::rampUpkeepPerQuarter();
		else if (id == "housekeeping")
			total += 10000;
		else if (id == "metro")
			total += 100000;
		else if (id == "security" || id == "secom")
			total += 20000;
		else if (id == "recycling")
			total += 50000;
	}
	return total;
}

int Game::quarterlyUpkeepTotal() const { return quarterlyMaintenanceCost(this); }

static void settleQuarterlyAccounting(Game *game)
{
	// The EXE opens a new quarter at 4:59AM, then processes departures,
	// rent and infrastructure. Never perform this more than once for the
	// same three-day financial cycle.
	// (finalizeQuarter() has already run, before the tenant judge, so
	// buybacks from move-outs land in the new quarter.)
	for (Game::ItemSet::const_iterator it = game->items.begin();
	     it != game->items.end(); ++it)
	{
		Item::Item *item = *it;
		if (item->underConstruction) continue;
		const std::string &id = item->prototype->id;
		if (id == "shop")
		{
			Item::Shop *shop = dynamic_cast<Item::Shop *>(item);
			if (!shop || !shop->let) continue;
		}
		else if (id == "office")
		{
			// EXE 1138:0bad pays while the tenant status is < 16 (including
			// workers away overnight); abandoned offices pay nothing.
			if (!item->isOccupied()) continue;
		}
		else
		{
			// Condos pay once at sale (move-in), never quarterly; hotel
			// rooms pay at checkout.
			continue;
		}
		int cls = openSkyDefaultRentClass(id);
		if (id == "shop")
		{
			Item::Shop *shop = dynamic_cast<Item::Shop *>(item);
			if (!shop || !shop->let) continue;
			cls = shop->tenant.rentClass;
		}
		else if (id == "office")
		{
			cls = item->tenant.rentClass;
		}
		const int rent = openSkyQuarterlyRent(id, cls);
		if (rent > 0)
			game->transferFunds(rent, "rent_income", "Quarterly rent income");
	}
	const int maintenance = quarterlyMaintenanceCost(game);
	if (maintenance > 0)
		game->transferFunds(-maintenance, "maintenance", "Quarterly maintenance");
}

// YootTower quarter (plan step 7, E3): T2.exe FUN_0041dc40 opens the new
// quarter's books (T2Settlement::Update) and charges
// T2FloorInfo::CalcMentenanceCost at midnight of day 1. Each item pays its
// TNDF/MVDF outMoney: lobbies per unit of width, movers per used car
// (oracle: Sample Tokyo Map's Simple Elevator paid 2 x $12,000 for 2 of 6
// car slots), stairs/escalators as 2 modules halved by attr 0x2000.
// Office rent and commercial income are not part of this settlement.
static void settleYootQuarter(Game *game)
{
	game->money.finalizeQuarter();
	int total = 0;
	for (Game::ItemSet::const_iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item *item = *it;
		const YootItemStats *stats = item->prototype->yoot;
		if (!stats || item->underConstruction) continue;
		int cars = 0;
		if (item->isElevator())
		{
			Item::Elevator::Elevator *e = static_cast<Item::Elevator::Elevator *>(item);
			cars = (int)e->cars.size();
		}
		else if (item->isStairlike())
			cars = 2;
		total += Yoot::maintenanceCost(*stats, item->size.x, cars);
	}
	if (total > 0)
		game->transferFunds(-total, "maintenance", "Quarterly maintenance");
}

// YootTower office rent (plan step 7, E4; Office.t2p FUN_1000c0c0): every
// let office pays 3 x inMoney[selection] once per quarter between 04:52
// and 04:59 on day 1. Oracle (Sample Tokyo Map.bld): 9 offices at
// inMoneyIndex 2 paid 9 x 3 x $3,000 = $81,000 by 05:24.
static void collectYootOfficeRent(Game *game)
{
	// Apartments pay 3 x inMoney the same way (oracle: settlement 1102
	// -21 = 3 x 7 units at inMoneyIndex 2).
	int total = 0, apartments = 0;
	for (const char * kind : {"office", "apartment"})
	{
		Game::ItemSetByString::const_iterator bucket = game->itemsByType.find(kind);
		if (bucket == game->itemsByType.end()) continue;
		for (Game::ItemSet::const_iterator it = bucket->second.begin(); it != bucket->second.end(); ++it)
		{
			Item::Item *tenant = *it;
			if (tenant->underConstruction || !tenant->isOccupied() || !tenant->prototype->yoot) continue;
			// Statuses 9-11 (no workers yet) take no quarterly rent.
			if (std::string(kind) == "office" && tenant->population == 0) continue;
			const int rent = Yoot::officeQuarterRent(*tenant->prototype->yoot, game->yootPriceSelection(tenant));
			if (std::string(kind) == "apartment") apartments += rent;
			else total += rent;
		}
	}
	game->yootBook(total, "rent_income", "Income from office rentals just came in.");
	game->yootBook(apartments, "rent_income", ""); // generic tenant payment: no message string
}

static double autosaveWallClock()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static bool allowsFloorSpan(const BuildRules & rules, int floor, int minX, int maxX)
{
	const long long width = (long long)maxX - minX;
	return width > 0 && width <= INT_MAX &&
		rules.allowsFootprint(minX, floor, (int)width);
}

// Wave U row 51 (Fast Mode): speed index 4 tops the ladder. It bypasses
// the one-sim-tick-per-frame throttle by running kFastTicksPerFrame full
// sim ticks per rendered frame (each paced like a top-speed tick) and
// skips Time's animation ramp (Game::advance / Game::setSpeedMode). The
// EXE toolbox speed sheet holds only four 23x24 cells, so its button
// reuses the top cell's art with a text label.
static const int    kFastSpeedMode     = 4;
static const double kFastSpeed         = 36.0; // 3x the paced top speed (12)
static const int    kFastTicksPerFrame = 3;

static bool canExtendFloor(const Game & game, int floor, int minX, int maxX)
{
	if (maxX <= minX) return false;
	Game::FloorItems::const_iterator existing = game.floorItems.find(floor);
	if (existing != game.floorItems.end())
	{
		minX = std::min(minX, existing->second->position.x);
		maxX = std::max(maxX, existing->second->getRect().maxX());
	}
	return allowsFloorSpan(game.activeRules, floor, minX, maxX);
}

void Game::askEventChoice(const std::string & title, const std::string & text,
                          const std::string & yesLabel, const std::string & noLabel,
                          const std::function<void(bool)> & onChoice)
{
	closeEventChoice();
	eventChoiceSavedSpeed = speedMode;
	setSpeedMode(0); // hold the sim while the player decides

	tgui::ChildWindow::Ptr window = tgui::ChildWindow::create(title);
	window->setClientSize({360 * app.uiScale, 130 * app.uiScale});
	window->setPosition({(app.window.getSize().x - 360 * app.uiScale) / 2.f,
	                     (app.window.getSize().y - 130 * app.uiScale) / 2.f});
	window->setResizable(false);
	window->setTitleButtons(tgui::ChildWindow::TitleButton::None);

	tgui::Label::Ptr label = tgui::Label::create(text);
	label->setPosition({14 * app.uiScale, 12 * app.uiScale});
	label->setTextSize(13 * app.uiScale);
	window->add(label);

	tgui::Button::Ptr yes = tgui::Button::create(yesLabel);
	yes->setSize({160 * app.uiScale, 30 * app.uiScale});
	yes->setPosition({20 * app.uiScale, 85 * app.uiScale});
	yes->onPress([this]() { answerEventChoice(true); });
	window->add(yes);

	tgui::Button::Ptr no = tgui::Button::create(noLabel);
	no->setSize({160 * app.uiScale, 30 * app.uiScale});
	no->setPosition({180 * app.uiScale, 85 * app.uiScale});
	no->onPress([this]() { answerEventChoice(false); });
	window->add(no);

	app.gui.add(window);
	eventChoiceWindow = window;
	eventChoiceCallback = onChoice;
}

void Game::askEventText(const std::string & title, const std::string & text,
                        const std::string & defaultText,
                        const std::function<void(const std::string &)> & onText)
{
	askEventChoice(title, text, "OK", "", nullptr);
	tgui::Container::Ptr window = eventChoiceWindow;
	window->removeAllWidgets();
	tgui::Label::Ptr label = tgui::Label::create(text);
	label->setPosition({14 * app.uiScale, 12 * app.uiScale});
	label->setTextSize(13 * app.uiScale);
	window->add(label);
	tgui::EditBox::Ptr edit = tgui::EditBox::create();
	edit->setText(defaultText);
	edit->setSize({330 * app.uiScale, 26 * app.uiScale});
	edit->setPosition({14 * app.uiScale, 44 * app.uiScale});
	window->add(edit);
	tgui::Button::Ptr ok = tgui::Button::create("OK");
	ok->setSize({160 * app.uiScale, 30 * app.uiScale});
	ok->setPosition({100 * app.uiScale, 85 * app.uiScale});
	ok->onPress([this, edit]() { if (!edit->getText().empty()) answerEventChoice(true); });
	window->add(ok);
	eventChoiceCallback = [edit, onText](bool) { onText(edit->getText().toStdString()); };
}

bool Game::qaBuild(const std::string & id, int2 pos)
{
	std::map<std::string, Item::AbstractPrototype *>::iterator p = itemFactory.prototypesById.find(id);
	if (p == itemFactory.prototypesById.end() || !p->second) return false;
	Item::AbstractPrototype * previous = toolPrototype;
	toolPrototype = p->second;
	const size_t before = items.size();
	const int fundsBefore = funds;
	constructToolAt(pos);
	toolPrototype = previous;
	const bool built = items.size() != before || funds != fundsBefore;
	LOG(IMPORTANT, "qa-build %s at %d,%d: %s", id.c_str(), pos.x, pos.y, built ? "built" : "refused");
	return built;
}

void Game::answerEventChoice(bool yes)
{
	if (!eventChoiceCallback) return;
	std::function<void(bool)> cb = eventChoiceCallback;
	closeEventChoice();
	cb(yes);
}

void Game::closeEventChoice()
{
	if (eventChoiceWindow) {
		app.gui.remove(eventChoiceWindow);
		eventChoiceWindow.reset();
	}
	eventChoiceCallback = nullptr;
	if (eventChoiceSavedSpeed >= 0) {
		setSpeedMode(eventChoiceSavedSpeed);
		eventChoiceSavedSpeed = -1;
	}
}

// ------------------------------------------------------------- debug menu

/// Hidden dev menu mirroring the EXE's debug actions 9000 (Terrorist),
/// 9001 (Treasure), 9002 (Fire) and 9003 (Redraw). Both this window and
/// its Ctrl+Shift+D keybind are unreachable unless the app was started
/// with --debug-menu.
void Game::toggleDebugMenu()
{
	if (!app.debugMenuEnabled()) return;

	if (debugMenuWindow) {
		app.gui.remove(debugMenuWindow);
		debugMenuWindow.reset();
		return;
	}

	tgui::ChildWindow::Ptr window = tgui::ChildWindow::create("Debug");
	window->setClientSize({150 * app.uiScale, 160 * app.uiScale});
	window->setPosition({(app.window.getSize().x - 150 * app.uiScale) / 2.f, 60 * app.uiScale});
	window->setResizable(false);
	window->setTitleButtons(tgui::ChildWindow::TitleButton::None);

	tgui::Button::Ptr terrorist = tgui::Button::create("Terrorist");
	terrorist->setSize({126 * app.uiScale, 28 * app.uiScale});
	terrorist->setPosition({12 * app.uiScale, 12 * app.uiScale});
	terrorist->onPress([this] { eventSystem.debugTriggerTerror(); });
	window->add(terrorist);

	tgui::Button::Ptr treasure = tgui::Button::create("Treasure");
	treasure->setSize({126 * app.uiScale, 28 * app.uiScale});
	treasure->setPosition({12 * app.uiScale, 48 * app.uiScale});
	treasure->onPress([this] { eventSystem.debugTriggerTreasure(); });
	window->add(treasure);

	tgui::Button::Ptr fire = tgui::Button::create("Fire");
	fire->setSize({126 * app.uiScale, 28 * app.uiScale});
	fire->setPosition({12 * app.uiScale, 84 * app.uiScale});
	fire->onPress([this] { eventSystem.debugTriggerFire(); });
	window->add(fire);

	tgui::Button::Ptr redraw = tgui::Button::create("Redraw");
	redraw->setSize({126 * app.uiScale, 28 * app.uiScale});
	redraw->setPosition({12 * app.uiScale, 120 * app.uiScale});
	redraw->onPress([this] { debugRedraw(); });
	window->add(redraw);

	app.gui.add(window);
	debugMenuWindow = window;
}

/// EXE debug action 9003: the original forced a full repaint of its
/// dirty-rect GDI screen. Our loop already repaints every frame, so the
/// cheapest faithful equivalent is clearing and pushing one complete
/// frame right now.
void Game::debugRedraw()
{
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	app.window.resetGLStates();
	app.gui.draw();
	app.window.display();
}

Game::Game(Application & app)
:	State("game"),
	app(app),
	activeRules(app.makeBuildRules(app.selectedRuleset(), app.selectedWorld())),
	skin(app.selectedSkin()),
	itemFactory(this),
	toolboxWindow(this),
	timeWindow(this),
	inspectorDialog(this),
	elevatorDialog(this),
	mapWindow(this),
	financeWindow(this),
	sky(this),
	lighting(this),
	decorations(this),
	vipSystem(this),
	eventSystem(this),
	levelUpDialog(this),
	reportDialog(this)
{

	// The EXE-sheet install at Application::init ran before any menu skin
	// selection could apply (the default skin is OpenSky, so the install
	// gate left first-party-covered keys empty). When this game runs the
	// Classic skin, re-run the loader NOW — after the selection, before
	// the first texture request (reloadGUI below). Idempotent: it only
	// overwrites cache entries. BitmapManager must learn the skin first:
	// the install gate reads it.
	if (!app.hasSkinAssets(skin))
	{
		LOG(WARNING, "Skin '%s' assets are unavailable; falling back to OpenSky art", skinDisplayName(skin));
		skin = Skin::OpenSky;
	}
	if (app.bitmaps.skin() != skin)
	{
		app.bitmaps.clear();
		app.bitmaps.setSkin(skin);
	}
	if (skin == Skin::Classic && app.hasSkinAssets(Skin::Classic))
	{
		SimTowerLoader loader(&app);
		loader.load();
	}
	//mapWindow     = NULL;

	// YootTower floor holes: same-floor walking between two routing nodes
	// requires continuous slab coverage (a hole is a visible gap). Under
	// SimTower/OpenSky the test always answers walkable, which reproduces
	// the previous link-everything graph exactly.
	gameMap.horizontalWalkable = [this](int y, int x1, int x2) {
		if (!activeRules.profile().yootWorld) return true;
		const int lo = std::min(x1, x2), hi = std::max(x1, x2);
		Item::Floor * slab = NULL;
		FloorItems::const_iterator fi = floorItems.find(y);
		if (fi != floorItems.end()) slab = fi->second;
		for (int x = lo; x <= hi; x++) {
			if (slab && slab->coversCell(x)) continue;
			// Lobby cells stay walkable (the atrium is open floor).
			ItemSetByInt::const_iterator row = itemsByFloor.find(y);
			bool lobby = false;
			if (row != itemsByFloor.end())
				for (ItemSet::const_iterator i = row->second.begin(); i != row->second.end(); ++i)
					if ((*i)->prototype->icon == ICON_LOBBY &&
					    x >= (*i)->position.x && x < (*i)->position.x + (*i)->size.x) { lobby = true; break; }
			if (!lobby) return false;
		}
		return true;
	};

	setRules(activeRules);
	funds  = activeRules.profile().startFunds;
	money.setBalance(funds);
	rating = activeRules.profile().startRating;
	population = 0;
	countT = TenantRules::CountT();
	populationNeedsUpdate = false;
	yootVipFlags = 0;
	yootSecretItems = 0;
	activeRules.setSecretUnlocked(false);
	lastAutosaveDay = (int)floor(time.absolute);
	lastAutosaveWallClock = autosaveWallClock();

	if (activeRules.profile().yootWorld)
		time.set(activeRules.profile().startDay + Time::hourToAbsolute(7.0));
	else
		time.set(7/78.0);
	lastAccountingDay = (int)floor(time.absolute);
	lastJudgeDay = (int)floor(time.absolute) - (time.hour < 5.0 ? 1 : 0);
	// Store the absolute three-day financial cycle, not Time::quarter's
	// presentation value. This survives year wrap without duplicate billing.
	lastAccountingQuarter = (int)floor(time.absolute / 3.0);
	yootRentCycle = lastAccountingQuarter -
		((time.day > 0 || time.hour * 60.0 >= Yoot::kOfficeRentFirstMinute) ? 0 : 1);
	itemBelowCursor = NULL;
	toolPrototype = NULL;
	statusMode = kNormal;

	zoom = 0.5;
	poi.y = 200;
	poi.x = 0;

	draggingElevator = NULL;
	draggingMotor = 0;
	viewportDrag = ViewportDrag::None;

	mainLobby = NULL;
	metroStation = NULL;

	itemFactory.loadPrototypes();
	applyItemCatalog();

	/*Item::Item * i = itemFactory.prototypes.front()->make(this);
	addItem(i);
	i = itemFactory.prototypes.front()->make(this);
	i->setPosition(int2(20, 0));
	addItem(i);*/
	/*Sprite * s = new Sprite;
	s->SetImage(app.bitmaps["simtower/security"]);
	s->Resize(384, 24);
	s->SetCenter(0, 24);
	s->SetPosition(0, 0);
	sprites.insert(s);*/

	reloadGUI();

	cockSound.setBuffer(app.sounds["simtower/cock"]);
	morningSound.setBuffer(app.sounds["simtower/birds/morning"]);
	bellsSound.setBuffer(app.sounds["simtower/bells"]);
	eveningSound.setBuffer(app.sounds["simtower/birds/evening"]);

}

Game::~Game()
{
	clearWorld();
}

void Game::clearWorld()
{
	inspectorDialog.close();
	elevatorDialog.close();
	closeEventChoice();
	mapWindow.close();
	financeWindow.close();
	reportDialog.close();
	routeBuildQueue.clear();
    for (Person *p : people) {
        p->journey.cancel();
        if (Item::Office::Worker *w = dynamic_cast<Item::Office::Worker *>(p)) w->parkingUsed = NULL;
        if (Item::Hotel::Guest *g = dynamic_cast<Item::Hotel::Guest *>(p)) g->parkingUsed = NULL;
    }
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		delete *i;
	}
	items.clear();
	itemsByFloor.clear();
	itemsByType.clear();
	floorItems.clear();
	mainLobby = NULL;
	metroStation = NULL;
	lobbyHeightLock = 0;
	itemBelowCursor = NULL;
	for (auto &zone : venueLists) for (auto &list : zone) list.clear();
	medicalAdequate = true; recyclingAdequate = true; recyclingDisplayStage = 0;
	population = 0;
	countT = TenantRules::CountT();
	populationNeedsUpdate = false;
	visualizeRoute.clear();
	gameMap.clear();
	decorations.reset();

	draggingElevator = NULL;
	draggingMotor = 0;
	draggingElevatorStart = 0;
	draggingElevatorLower = false;
	viewportDrag = ViewportDrag::None;
	toolPrototype = NULL;

	cockSound.Stop();
	morningSound.Stop();
	bellsSound.Stop();
	eveningSound.Stop();
	while (!autoreleaseSounds.empty()) {
		Sound * sound = *autoreleaseSounds.begin();
		sound->Stop();
		autoreleaseSounds.erase(autoreleaseSounds.begin());
		delete sound;
	}
	playingSounds.clear();
	soundPlayTimes.clear();
	vipSystem.reset();
	eventSystem.reset();
	levelUpDialog.close();
}

void Game::activate()
{
	State::activate();
}

void Game::deactivate()
{
	State::deactivate();
}

bool Game::handleEvent(sf::Event & event)
{

	if (const auto *wheelEvent = event.getIf<sf::Event::MouseWheelScrolled>()) {
		sf::Vector2i mousePosition = wheelEvent->position;
		tgui::Vector2f mousePosF(mousePosition.x, mousePosition.y);
		if (app.gui.getWidgetAtPos(mousePosF, true) != nullptr)
			return false;
		if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(mousePosF))
			return false;
		if (timeWindow.window && timeWindow.window->isMouseOnWidget(mousePosF))
			return false;
		if (app.menu && app.menu->isMouseOnWidget(mousePosF))
			return false;

		bool isShiftPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
		bool isCtrlPressed = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl);
		float delta = wheelEvent->delta;
		if (isCtrlPressed && wheelEvent->wheel == sf::Mouse::Wheel::Vertical) {
			// Ctrl + wheel: zoom toward the cursor (~1.25x per notch).
			// Plain wheel keeps scrolling the viewport.
			zoomBy(std::pow(1.25, -delta), &mousePosition);
			return true;
		}
		if (wheelEvent->wheel == sf::Mouse::Wheel::Horizontal || isShiftPressed) {
			poi.x -= delta * 40.f * zoom;
			poi.x = std::max(-horizontalScrollLimit(), std::min(horizontalScrollLimit(), poi.x));
		} else {
			poi.y += delta * 40.f * zoom;
		}
		return true;
	}

	enum class EventType {
		None,
		KeyPressed,
		TextEntered,
		MouseButtonPressed,
		MouseMoved,
		MouseButtonReleased
	};

	EventType eventType = EventType::None;
	sf::Keyboard::Key keyCode = sf::Keyboard::Key::Unknown;
	char32_t textUnicode = 0;
	sf::Vector2i mousePosition{};
	bool keyControl = false, keyShift = false;

	if (const auto * keyPressed = event.getIf<sf::Event::KeyPressed>()) {
		eventType = EventType::KeyPressed;
		keyCode = keyPressed->code;
		keyControl = keyPressed->control;
		keyShift = keyPressed->shift;
	} else if (const auto * textEntered = event.getIf<sf::Event::TextEntered>()) {
		eventType = EventType::TextEntered;
		textUnicode = textEntered->unicode;
	} else if (const auto * mouseButtonPressed = event.getIf<sf::Event::MouseButtonPressed>()) {
		eventType = EventType::MouseButtonPressed;
		mousePosition = mouseButtonPressed->position;
	} else if (const auto * mouseMoved = event.getIf<sf::Event::MouseMoved>()) {
		eventType = EventType::MouseMoved;
		mousePosition = mouseMoved->position;
	} else if (const auto * mouseButtonReleased = event.getIf<sf::Event::MouseButtonReleased>()) {
		eventType = EventType::MouseButtonReleased;
		mousePosition = mouseButtonReleased->position;
	}

	switch (eventType) {
		case EventType::KeyPressed: {
			switch (keyCode) {
				case sf::Keyboard::Key::Left:  poi.x = std::max(-horizontalScrollLimit(), poi.x - 20); return true;
				case sf::Keyboard::Key::Right: poi.x = std::min(horizontalScrollLimit(), poi.x + 20); return true;
				case sf::Keyboard::Key::Up:    poi.y += 20; return true;
				case sf::Keyboard::Key::Down:  poi.y -= 20; return true;
				case sf::Keyboard::Key::F1:    reloadGUI(); return true;
				case sf::Keyboard::Key::F3:    setRating(1); return true;
				case sf::Keyboard::Key::F2:
					writeTowerFile("default.tower");
					return true;
		case sf::Keyboard::Key::PageUp:   zoomBy(0.5); return true;
		case sf::Keyboard::Key::PageDown: zoomBy(2.0); return true;
		case sf::Keyboard::Key::O:        cycleStatusMode(); return true;
		case sf::Keyboard::Key::M:
			mapWindow.setVisible(!mapWindow.isVisible());
			return true;
		case sf::Keyboard::Key::F:
			financeWindow.setVisible(!financeWindow.isVisible());
			return true;
		case sf::Keyboard::Key::P:
			showReport();
			return true;
		case sf::Keyboard::Key::D:
			// Hidden debug menu (EXE actions 9000-9003): Ctrl+Shift+D,
			// only when the app was started with --debug-menu.
			if (keyControl && keyShift && app.debugMenuEnabled()) {
				toggleDebugMenu();
				return true;
			}
			break;
		case sf::Keyboard::Key::Escape:
			if (inspectorDialog.isVisible()) { inspectorDialog.close(); return true; }
			if (elevatorDialog.isVisible())  { elevatorDialog.close();  return true; }
			if (debugMenuWindow) { app.gui.remove(debugMenuWindow); debugMenuWindow.reset(); return true; }
			if (financeWindow.isVisible())   { financeWindow.setVisible(false); return true; }
			if (reportDialog.isVisible())    { reportDialog.setVisible(false); return true; }
			if (mapWindow.isVisible())       { mapWindow.setVisible(false); return true; }
			break;
			default: break;
		}
		} break;

		case EventType::TextEntered: {
			switch (textUnicode) {
				case '0': setSpeedMode(0); return true;
				case '1': setSpeedMode(1); return true;
				case '2': setSpeedMode(2); return true;
				case '3': setSpeedMode(3); return true;
				case '4': setSpeedMode(4); return true;
				default: break;
			} break;
		} break;

		case EventType::MouseButtonPressed: {
			tgui::Vector2f mousePosF(mousePosition.x, mousePosition.y);
			if (app.gui.getWidgetAtPos(mousePosF, true) != nullptr)
				return false;
			if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(mousePosF))
				return false;

			// Emergency events take priority over all other click actions.
			// If a fire or bomb is active, clicking the event location
			// dispatches the helicopter / defuses the bomb.
			if (eventSystem.isActive()) {
				if (eventSystem.handleClick(toolPosition))
					return true;
			}

            if ((selectedTool == "bulldozer" || selectedTool.find("item-") == 0) &&
                rejectDisasterWorldEdit()) return true;
            if (selectedTool == "bulldozer" && eventSystem.clearRubble(toolPosition.y, toolPosition.x)) {
                playOnce("simtower/bulldozer");
                timeWindow.showMessage("Burned area cleared.");
                return true;
            }
			float2 mousePoint(mousePosition.x, mousePosition.y);
			//rectf toolboxWindowRect(float2(toolboxWindow.window->getAbsolutePosition().x, toolboxWindow.window->getAbsolutePosition().y), float2(toolboxWindow.window->GetClientWidth(), toolboxWindow.window->GetClientHeight()));
			// rectf timeWindowRect(float2(timeWindow.window->GetAbsoluteLeft(), timeWindow.window->GetAbsoluteTop()), float2(timeWindow.window->GetClientWidth(), timeWindow.window->GetClientHeight()));
			// rectf mapWindowRect(float2(mapWindow->GetAbsoluteLeft(), mapWindow->GetAbsoluteTop()), float2(mapWindow->GetClientWidth(), mapWindow->GetClientHeight()));

			// Prevent construction or triggering of tool if mouse cursor within toolboxWindow
			//if (toolboxWindowRect.containsPoint(mousePoint)) break;

			// Prevent construction or triggering of tool if mouse cursor within timeWindow
			// if (timeWindowRect.containsPoint(mousePoint)) break;

			// Prevent construction or triggering of tool if mouse cursor within mapWindow
			// if (mapWindowRect.containsPoint(mousePoint)) {
			// 	break;	// Break for now, may add code to handle viewport shift in future
			// }

			if (selectedTool.find("item-") == 0 && toolPrototype) {
				// Construction logic for item tools only
				double2 halfsize((app.window.getSize().x)*0.5*zoom, (app.window.getSize().y)*0.5*zoom);
				sf::FloatRect viewRect(
					{static_cast<float>(round(poi.x - halfsize.x)), static_cast<float>(round(-poi.y - halfsize.y))},
					{static_cast<float>(halfsize.x * 2), static_cast<float>(halfsize.y * 2)}
				);
				sf::View cameraView(viewRect);
				sf::Vector2f mp = app.window.mapPixelToCoords(mousePosition, cameraView);
				mp.y = -mp.y;

				int height = toolPrototype->size.y;
				int yHeightOffset = height;
				if (toolPrototype->icon == ICON_LOBBY) {
					height = lobbyToolHeight();
					yHeightOffset = 1;
				}
				toolPosition = int2(round(mp.x/8-toolPrototype->size.x/2.0), round(mp.y/36-yHeightOffset/2.0));

				// ISSUE-039 (OpenSkyWeb): batchable room tools start a preview-only
				// drag (ghosts mark each cell valid/red) and commit the whole
				// batch on mouse release. Non-batchable tools (elevators, lobbies,
				// floors, stairs, escalators, metro, parking ramp) keep instant
				// placement.
				if (isBatchDraggable(toolPrototype)) {
					startBatchDrag();
					return true;
				}
				constructToolAt(toolPosition);
				return true;
			}
			else if (itemBelowCursor) {
				if (selectedTool == "bulldozer") {
					if (activeRules.profile().yootWorld) {
						// YootTower: floors go top-down / bottom-up, parking from the outside (step 13).
						std::string yootReason;
						const bool yootOk = Yoot::removalAllowed(*this, itemBelowCursor, yootReason);
						const bool yootFloor = itemBelowCursor->prototype->icon == ICON_FLOOR;
						if (!yootOk || (!yootFloor && (itemBelowCursor->prototype->icon == ICON_LOBBY || itemBelowCursor->prototype->icon == ICON_FLOOR || itemBelowCursor->prototype->icon == ICON_METRO))) {
							playOnce("simtower/construction/impossible");
							timeWindow.showMessage(yootOk ? "Cannot bulldoze " + itemBelowCursor->prototype->name : yootReason);
							break;
						}
						LOG(DEBUG, "destroy %s", itemBelowCursor->desc().c_str());
						bool canHaulPeople = false;
						if (itemBelowCursor->canHaulPeople()) canHaulPeople = true;
						removeItem(itemBelowCursor);
						if (canHaulPeople) updateRoutes();
						playOnce("simtower/bulldozer");
					}
					else {
						// SimTower/OpenSky: EXE demolish rules (art batch 6).
						bulldozeItem(itemBelowCursor);
					}
				}
				else if (selectedTool == "finger") {
					if (itemBelowCursor->prototype->id.find("elevator") == 0) {
						Item::Elevator::Elevator * e = (Item::Elevator::Elevator *)itemBelowCursor;

						draggingMotor = 0;
						if (toolPosition.y < itemBelowCursor->position.y) draggingMotor = -1;
						if (toolPosition.y >= itemBelowCursor->position.y + itemBelowCursor->size.y) draggingMotor = 1;

						// Shaft resizing is a world edit too: locked during a fire/bomb (EXE 1060:004E).
						if (draggingMotor != 0 && rejectDisasterWorldEdit()) draggingMotor = 0;
						if (draggingMotor != 0) {
							LOG(DEBUG, "drag elevator %s motor %i", itemBelowCursor->desc().c_str(), draggingMotor);
							draggingElevator = e;
							draggingElevatorStart = toolPosition.y;
							if (draggingElevatorStart < draggingElevator->position.y) {
								draggingElevatorLower = true;
								draggingElevatorStart++;
							} else {
								draggingElevatorLower = false;
								draggingElevatorStart--;
							}
						} else {
							LOG(DEBUG, "clicked elevator %s on floor %i", itemBelowCursor->desc().c_str(), toolPosition.y);
							if (e->unservicedFloors.count(toolPosition.y) && !e->canAddStop()) {
								// YootTower Large Elevator stop cap (MVDF maxStop).
								timeWindow.showMessage("This elevator cannot stop at any more floors.");
							} else if (!e->unservicedFloors.erase(toolPosition.y)) {
								e->unservicedFloors.insert(toolPosition.y);
								gameMap.removeNode(MapNode::Point(e->position.x + e->size.x/2, toolPosition.y), itemBelowCursor);
							} else {
								gameMap.addNode(MapNode::Point(e->position.x + e->size.x/2, toolPosition.y), itemBelowCursor);
							}
							e->cleanQueues();
							updateRoutes();
						}
					}
				}
			else if (selectedTool == "inspector") {
				LOG(DEBUG, "inspect %s", itemBelowCursor->desc().c_str());
				visualizeRoute = itemBelowCursor->lobbyRoute;
				inspectorDialog.showForItem(itemBelowCursor);
			}
			}
		} break;

case EventType::MouseMoved: {
if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
return false;

if (batchDragActive()) updateBatchDrag();
if (draggingElevator &&
    !activeRules.allowsFootprint(draggingElevator->position.x, draggingElevator->position.y,
                                 draggingElevator->size.x, draggingElevator->size.y)) {
	timeWindow.showMessage("Cannot extend elevator outside the " +
		std::string(activeRules.displayName()) + " build boundary.");
	draggingElevator = NULL;
}


if (draggingElevator && draggingElevator->repositionMotor(draggingMotor, toolPosition.y)) {
				// Construct floors
				if (draggingElevatorLower) {
					if (draggingElevatorStart > draggingElevator->position.y) {
						for(int i = draggingElevator->position.y; i < draggingElevatorStart ; i++) {
							extendFloor(i, draggingElevator->position.x, draggingElevator->getRect().maxX());
						}
					}
				} else {
					recti rect = draggingElevator->getRect();
					if (draggingElevatorStart < rect.maxY() - 1) {
						for(int i = rect.maxY() - 1; i > draggingElevatorStart; i--) {
							extendFloor(i, draggingElevator->position.x, draggingElevator->getRect().maxX());
						}
					}
				}

				eventSystem.notifyPlaced(); // shaft extension counts as a build
				// Update PathFinder map
				gameMap.handleElevatorResize(draggingElevator, draggingElevatorLower, draggingElevatorStart);

				updateRoutes();
			}
		} break;

case EventType::MouseButtonReleased: {
if (toolboxWindow.window && toolboxWindow.window->isMouseOnWidget(tgui::Vector2f(mousePosition.x, mousePosition.y)))
return false;

// ISSUE-039: commit the whole batch now (nothing was built/charged while dragging).
if (batchDragActive()) { finishBatchDrag(); return true; }

draggingElevator = NULL;
} break;
		case EventType::None:
			break;
	}
	return false;
}

bool Game::handleViewportScrollbarEvent(sf::Event &event)
{
	sf::Vector2u ws = app.window.getSize();
	const int scrollbarSize = 16;
	const int topOffset = 23;
	const int rightX = static_cast<int>(ws.x) - scrollbarSize;
	const int bottomY = static_cast<int>(ws.y) - scrollbarSize;

	auto setVerticalFromMouse = [&](int y) {
		double usable = std::max(1, static_cast<int>(ws.y) - topOffset - scrollbarSize);
		double t = std::max(0.0, std::min(1.0, (y - topOffset) / usable));
		poi.y = 360 * 12 - t * (360 * 13);
	};
	auto setHorizontalFromMouse = [&](int x) {
		double usable = std::max(1, static_cast<int>(ws.x) - scrollbarSize);
		double t = std::max(0.0, std::min(1.0, x / usable));
		poi.x = horizontalScrollLimit() * (2 * t - 1);
	};

	if (const auto *pressed = event.getIf<sf::Event::MouseButtonPressed>()) {
		const sf::Vector2i p = pressed->position;
		if (p.x >= rightX && p.y >= topOffset && p.y < bottomY) {
			viewportDrag = ViewportDrag::Vertical;
			setVerticalFromMouse(p.y);
			return true;
		}
		if (p.y >= bottomY && p.x < rightX) {
			viewportDrag = ViewportDrag::Horizontal;
			setHorizontalFromMouse(p.x);
			return true;
		}
	}
	if (const auto *moved = event.getIf<sf::Event::MouseMoved>()) {
		if (viewportDrag == ViewportDrag::Vertical) {
			setVerticalFromMouse(moved->position.y);
			return true;
		}
		if (viewportDrag == ViewportDrag::Horizontal) {
			setHorizontalFromMouse(moved->position.x);
			return true;
		}
	}
	if (event.is<sf::Event::MouseButtonReleased>()) {
		if (viewportDrag != ViewportDrag::None) {
			viewportDrag = ViewportDrag::None;
			return true;
		}
	}
	return false;
}

void Game::drawViewportScrollbars(sf::RenderTarget &target) const
{
	sf::Vector2u ws = app.window.getSize();
	const float scrollbarSize = 16.f;
	const float topOffset = 23.f;
	const float rightX = static_cast<float>(ws.x) - scrollbarSize;
	const float bottomY = static_cast<float>(ws.y) - scrollbarSize;

	sf::View previous = target.getView();
	target.setView(sf::View(sf::FloatRect({0.f, 0.f}, {static_cast<float>(ws.x), static_cast<float>(ws.y)})));

	sf::RectangleShape track;
	track.setFillColor(sf::Color(198, 198, 198));
	track.setOutlineColor(sf::Color(32, 32, 32));
	track.setOutlineThickness(1.f);
	track.setSize({scrollbarSize, bottomY - topOffset});
	track.setPosition({rightX, topOffset});
	target.draw(track);

	double verticalRange = 360 * 13;
	double vt = std::max(0.0, std::min(1.0, (360 * 12 - poi.y) / verticalRange));
	sf::RectangleShape thumb;
	thumb.setFillColor(sf::Color(145, 145, 145));
	thumb.setOutlineColor(sf::Color(32, 32, 32));
	thumb.setOutlineThickness(1.f);
	thumb.setSize({scrollbarSize - 2.f, 96.f});
	thumb.setPosition({rightX + 1.f, topOffset + static_cast<float>(vt * std::max(1.f, bottomY - topOffset - 96.f))});
	target.draw(thumb);

	track.setSize({rightX, scrollbarSize});
	track.setPosition({0.f, bottomY});
	target.draw(track);

	double ht = std::max(0.0, std::min(1.0, (poi.x / horizontalScrollLimit() + 1) / 2));
	thumb.setSize({128.f, scrollbarSize - 2.f});
	thumb.setPosition({static_cast<float>(ht * std::max(1.f, rightX - 128.f)), bottomY + 1.f});
	target.draw(thumb);

	target.setView(previous);
}

double Game::horizontalScrollLimit() const
{
	// YootTower: OpenSky x = 0 is the world centre (unit = x + width/2), so
	// the world spans +-width/2 units of 8 px (Hawaii's sea, Tokyo's edges).
	const RulesProfile & profile = activeRules.profile();
	if (profile.yootWorld && profile.worldWidth > 0)
		return std::max(512.0, profile.worldWidth * 8 * 0.5);
	return 512.0;
}

void Game::qaSetViewportScrollFractions(double horizontal, double vertical)
{
	horizontal = std::max(0.0, std::min(1.0, horizontal));
	vertical = std::max(0.0, std::min(1.0, vertical));
	poi.x = horizontalScrollLimit() * (2 * horizontal - 1);
	poi.y = 360 * 12 - vertical * (360 * 13);
}

/// One simulation tick (Wave U row 51, Fast Mode): the sim core that used
/// to be the head of Game::advance, split out so Fast can run several
/// full ticks per rendered frame. `dt` is this tick's slice of the frame
/// time; at one tick per frame it is the raw frame delta, exactly as before.
void Game::advanceSimTick(double dt)
{
	// Rebuild a slice of the invalidated lobby routes (see updateRoutes).
	pumpRouteBuilds();

    const bool eventWasActive = eventSystem.isActive();
	// Advance the clock first. Financial quarters are three-day cycles, and
	// settle at the 4:59AM/5:00AM boundary rather than at the midnight
	// quarter-number transition. The cycle marker makes repeated frames and
	// large time jumps idempotent.
	time.advance(dt);
	int currentAccountingDay = (int)floor(time.absolute);
	if (currentAccountingDay > lastAccountingDay) {
		settleDailyAccounting();
		lastAccountingDay = currentAccountingDay;
		autosaveIfDue();
	}
	const int financialCycle = (int)floor(time.absolute / 3.0);
	// YootTower settles at midnight of day 1 (FUN_0041dc40), SimTower at 5:00.
	const bool yootEconomy = activeRules.profile().yootWorld;
	const bool quarterlySettlementDue =
		financialCycle > lastAccountingQuarter && (yootEconomy || time.hour >= 5.0);

    eventSystem.advance(dt);
    const bool frozen = eventWasActive || eventSystem.isActive();
    // The EXE freezes the VIP system and recycling while a fire/bomb
    // emergency is active; YootTower keeps them running.
    if (!frozen || yootEconomy) vipSystem.advance(dt);
    if (!frozen || yootEconomy) Item::Recycling::advanceAll(this);

	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		Item::Item * item = *i;
		// Construction timer: clear the flag once elapsed and let the
		// item start ticking. Items still under construction are skipped
		// entirely - no people, no schedules, no rent. Wave U row 53: the
		// EXE plays no completion chime here (only the jackhammer while
		// building), so completion ends silently.
		if (item->underConstruction) {
			if (time.absolute >= item->constructionEndTime) {
				item->underConstruction = false;
			} else {
				continue;
			}
		}
		// YootTower BOEV remains active after the emergency escape until
		// power is restored.
		if (activeRules.profile().yootWorld && eventSystem.power.isBlackOut() &&
		    item->isElevator()) continue;
        // Empty transports still advance their cars, doors and timers; the metro
        // keeps its 07:00/17:00 open-close sweep (trains and riders are gated inside).
        if (!frozen || item->isElevator() || item->isStairlike() || item->prototype->id == "metro") item->advance(dt);
    }

    if (!frozen || yootEconomy) for (PersonSet::iterator p = people.begin(); p != people.end(); p++) {
        (*p)->advance(dt);
    }
	bool quarterFinalized = false;
	// Quarterly accounting is committed after the item/person pass. Office and
	// Condo use this boundary frame to remove unattractive tenants first, so
	// leavers do not pay the rent that follows.
	// Daily tenant judge on the 04:59 settle tick (first tick at/after 05:00
	// each day): per-floor judge, quarter-day move-out/settle or the arm
	// pass. It runs just BEFORE the quarterly rent loop so leavers do not pay.
	if (!yootEconomy && time.hour >= 5.0 && (int)floor(time.absolute) > lastJudgeDay)
	{
		lastJudgeDay = (int)floor(time.absolute);
		// Quarter day: open the new quarter FIRST so move-out buybacks are
		// booked in it, then judge (same predicate as the settlement), then
		// bill the survivors below.
		if (quarterlySettlementDue) { money.finalizeQuarter(); quarterFinalized = true; }
		judgeSystem.judgeDaily(this, quarterlySettlementDue);
		vipSystem.armDaily();
	}
	if (quarterlySettlementDue)
	{
		if (yootEconomy) {
			// T2 settlement at midnight of day 1; finalizes the quarter itself.
			settleYootQuarter(this);
		} else {
			if (!quarterFinalized) money.finalizeQuarter();
			settleQuarterlyAccounting(this);
		}
		lastAccountingQuarter = financialCycle;
		// The original pops its financial report right after the
		// quarterly settlement commits (Wave U row 49).
		showReport();
	}
	// YootTower office rent: day 1, from 04:52 (a tick that jumps over the
	// 8-minute window still pays once, late).
	if (yootEconomy && time.day == 0 && financialCycle > yootRentCycle &&
	    time.hour * 60.0 >= Yoot::kOfficeRentFirstMinute)
	{
		collectYootOfficeRent(this);
		yootRentCycle = financialCycle;
	}

	// Batch 5 hotel pass (T16). The restaurants open from their own advance();
	// then roach spread, JudgeAllHotel, the VIP day check and the hotel name purge.
	if (!frozen && time.checkHour(17)) {
		Item::Hotel::spreadRoaches(this);
		Item::Hotel::judgeAll(this);
		vipSystem.dayCheck();
		NameManager::purgeClass(NameManager::kNameHotel); // 17:00 hotel name purge (T47)
	}
	// ft 2500: visitor names are purged (T47).
	if (!frozen && Item::Item::visitsThisTick(this, 2600, 2500) > 0)
		NameManager::purgeClass(NameManager::kNameVisitor);
	// 07:00: venue zone lists are rebuilt and the medical flag is re-armed.
	if (!frozen && time.checkHour(7)) {
		rebuildVenueLists();
		if (rating >= 2) medicalAdequate = true;
	}

	if (populationNeedsUpdate) {
		populationNeedsUpdate = false;
		if (yootEconomy) {
			int p = 0;
			for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
				p += (*i)->population;
			}
			p += eventSystem.people.visitorPopulation(); // YootTower: the pool's visitors
			setPopulation(p);
		} else {
			// CountT is incremental (batch 5 T14): the global is countT.population.
			setPopulation(countT.population);
		}
	}

	//Play sounds. checkHour reads the clock transition of the last
	// Time::set, so hour-boundary events belong to the sim tick: a Fast
	// frame crosses up to three of them.
    if (!frozen) {
        if (time.checkHour(5))  cockSound.Play(this);
        if (time.checkHour(6))  morningSound.Play(this);
        if (time.checkHour(9))  bellsSound.Play(this);
        if (time.checkHour(18)) eveningSound.Play(this);
    }

	// Promotion landing window (LevelUp): 3->4 and 4->5 only land 5PM-7AM on
	// weekdays. Population/item events already re-evaluate; this catches a
	// tower that became eligible while the window was shut (e.g. a good VIP
	// review at 4PM) so it is crowned the moment 5PM opens.
	if (!activeRules.profile().grades.empty()) {
		// YootTower checks the grade gate every idle tick (T2.exe
		// FUN_0041da20 -> CheckGameLevel), with no time-of-day window.
		yootCheckGrade();
	} else if (time.checkHour(LevelUp::kPromotionWindowStartHour)) {
		ratingMayIncrease();
	}
}

void Game::advanceHeadless(double dt)
{
	if (eventChoiceCallback) {
		recordSimEvent("event choice answered: yes");
		answerEventChoice(true);
	}
	const int simTicks = (speedMode == kFastSpeedMode) ? kFastTicksPerFrame : 1;
	for (int tick = 0; tick < simTicks; tick++)
		advanceSimTick(dt / simTicks);
	sky.advance(dt);
}

void Game::advance(double dt)
{
	sf::RenderWindow & win = app.window;
	drawnSprites = 0;

	// Advance the sim. Fast (speed 4) bypasses the one-sim-tick-per-frame
	// throttle: several full sim ticks run per rendered frame, each paced
	// like a top-speed tick (see advanceSimTick / setSpeedMode). Every
	// other speed keeps the single tick per frame.
	const double absoluteAtFrameStart = time.absolute;
	const int simTicks = (speedMode == kFastSpeedMode) ? kFastTicksPerFrame : 1;
	for (int tick = 0; tick < simTicks; tick++)
		advanceSimTick(dt / simTicks);

	timeWindow.updateTime();
	timeWindow.advance(dt);
	levelUpDialog.advance(dt);
	sky.advance(dt);
	lighting.advance(dt);
	if (yootArt) {
		yootClock += dt;
		YootSkyInputs sky_in;
		sky_in.month = time.dayOfYear();
		sky_in.minuteOfDay = (int)floor(time.hour * 60);
		sky_in.rainy = sky.rainyDay;
		// T2PaletteAnime::ChangeSE: one SE step every 5 of the 30 Hz
		// IsSETime ticks, i.e. 6 steps a second.
		sky_in.seKind = (int)(yootClock * 6);
		yootArt->setOutside(statusMode == kOutside);
		yootArt->update(sky_in);
		// Plugin item sheets follow the tower palette (windows, signs).
		yootRecolourPluginViews(yootArt->palette(), yootArt->animStart(), yootArt->animCount());
	}
	toolboxWindow.update();

	// Refresh the minimap every ~1 second of game time (cheaper than every
	// frame, plenty for an overview that mostly changes on construction).
	// Compared against the frame-start clock so a Fast frame that crosses
	// several boundaries still refreshes exactly once.
	if (mapWindow.isVisible() &&
	    floor(time.absolute / (0.25 * Time::kBaseSpeed)) !=
	    floor(absoluteAtFrameStart / (0.25 * Time::kBaseSpeed)))
	{
		mapWindow.renderMap();
	}

	// Refresh the finance window on the same cadence; its figures only
	// change when money moves, so a ~1s throttle is plenty.
	if (financeWindow.isVisible() &&
	    floor(time.absolute / (0.25 * Time::kBaseSpeed)) !=
	    floor(absoluteAtFrameStart / (0.25 * Time::kBaseSpeed)))
	{
		financeWindow.refresh();
	}

	morningSound.setLooping(time.hour < 8);

	//Constrain the POI.
	double2 halfsize((win.getView().getSize().x)*0.5*zoom, (win.getView().getSize().y)*0.5*zoom);
	// The YootTower skin scrolls down to the world's bottom row (Kegon's
	// Observatory stands on B32, Tokyo's rock reaches B12).
	double bottom = -360;
	if (const YootWorldArt * art = yootSkinArt())
		bottom = std::min(bottom, -36.0 * (art->geometry().height - art->geometry().groundLine));
	poi.y = std::max<double>(std::min<double>(poi.y, 360*12 - halfsize.y), bottom + halfsize.y);
	poi.x = std::max<double>(std::min<double>(poi.x, horizontalScrollLimit()), -horizontalScrollLimit());

	//Adust the camera.
	sf::FloatRect view(
		{static_cast<float>(round(poi.x - halfsize.x)), static_cast<float>(round(-poi.y - halfsize.y))},
		{static_cast<float>(halfsize.x * 2), static_cast<float>(halfsize.y * 2)}
	);
	sf::View cameraView(view);
	win.setView(cameraView);

	//Prepare the current tool.
	sf::Vector2f mp = win.mapPixelToCoords(sf::Mouse::getPosition(win));
	mp.y = -mp.y;
	/* [TRICKY]
	 * The view as defined by SFML's sf::View uses a coordinate system that 
	 * has the origin in the top right corner and grows down and to the right.
	 * The rest of this application (that is, the positioning of the sprites)
	 * uses a coordinate system that has the origin in the bottom right corner
	 * and grows to the right.
	 * This was incredibly tricky for me to wrap my head around, but the only
	 * translation from one to the other that is needed is to negate the "top"
	 * of the view when doing the conditional drawing of on-screen objects.
	 */
	view.position.y = -view.position.y;
	Item::AbstractPrototype * previousPrototype = toolPrototype;
	if (selectedTool.find("item-") == 0) {
		toolPrototype = itemFactory.prototypesById[selectedTool.substr(5)];
		int height = toolPrototype->size.y;
		int yHeightOffset = height;
		if (toolPrototype->icon == ICON_LOBBY) {
			height = lobbyToolHeight();
			yHeightOffset = 1;
		}
		toolPosition = int2(round(mp.x/8-toolPrototype->size.x/2.0), round(mp.y/36-yHeightOffset/2.0));
	} else {
		toolPrototype = NULL;
		toolPosition = int2(floor(mp.x/8), floor(mp.y/36));
	}
	if (previousPrototype != toolPrototype) timeWindow.updateTooltip();

	//Draw the sky and decorations. The YootTower skin replaces the sky,
	//skyline and ground with the world plugin's backdrop.
	if (const YootWorldArt * art = yootSkinArt())
		art->drawBackdrop(win);
	else
		win.draw(sky);
	win.draw(decorations);
	eventSystem.drawBehindTower(win);

	//Draw the items that are in view.
	Item::Item * previousItemBelowCursor = itemBelowCursor;
	itemBelowCursor = NULL;

	// Skin::Yoot Outside view: the facades replace the tower interior.
	const bool yootOutside = statusMode == kOutside && yootSkinArt();
	if (yootOutside) drawYootOutView(*this, win);

	//Draw floor items first
	if (!yootOutside)
	for (ItemSet::iterator i = itemsByType["floor"].begin(); i != itemsByType["floor"].end(); i++) {
		const int2 & vp = (*i)->getPositionPixels();
		const sf::Vector2u & vs = (*i)->getSizePixels();
		if ((vp.x + vs.x >= view.position.x) && (vp.x <= (view.position.x + view.size.x)) &&
			((vp.y + vs.y) >= (view.position.y - view.size.y)) && (vp.y <= view.position.y)) {
			win.draw(**i);
			if ((*i)->containsPoint(double2(mp.x, mp.y))) itemBelowCursor = *i;
		}
	}

	//Draw the remaining items in the building
	for (int layer = 0; layer < 2 && !yootOutside; layer++) {
		if (layer == 1) eventSystem.drawUnderMovers(win);
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			if ((*i)->layer != layer) continue;
			const int2 & vp = (*i)->getPositionPixels();
			const sf::Vector2u & vs = (*i)->getSizePixels();
			if ((vp.x + vs.x >= view.position.x) && (vp.x <= (view.position.x + view.size.x)) &&
				((vp.y + vs.y) >= (view.position.y - view.size.y)) && (vp.y <= view.position.y)) {
				win.draw(**i);
				if ((*i)->containsPoint(double2(mp.x, mp.y))) itemBelowCursor = *i;
			}
		}
	}

	//Status overlay pass: tint tenant items according to statusMode. The
	//overlay is a translucent quad sized to the item. Only tenant types are
	//tinted - floors, elevators and lobbies are left alone.
	if (statusMode != kNormal) {
		for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
			Item::Item * item = *i;
			if (item->underConstruction) continue;
			const std::string & id = item->prototype->id;
			bool isTenant = (id == "office" || id == "condo" || id == "yoot_condo" ||
			                 id == "hotel_single" || id == "hotel_double" ||
			                 id == "hotel_suite" || id == "hotel" ||
			                 id == "fastfood" || id == "restaurant" ||
			                 id == "cinema"    || id == "partyhall");
			if (!isTenant) continue;

			sf::Color tint = sf::Color::Transparent;
			if (statusMode == kEval) {
				double e = item->evaluation;
				if      (e >= 70) tint = sf::Color(  0,  96, 255, 110); // blue = high
				else if (e >= 40) tint = sf::Color(255, 200,   0, 110); // yellow = medium
				else              tint = sf::Color(255,   0,   0, 110); // red = low
			} else if (statusMode == kHotel) {
				if (id == "hotel_single" || id == "hotel_double" ||
				    id == "hotel_suite"  || id == "hotel") {
					Item::Hotel * hotel = dynamic_cast<Item::Hotel *>(item);
					if (hotel) {
						if      (hotel->roomState == Item::Hotel::kDirty)    tint = sf::Color(255, 0, 0, 140);
						else if (hotel->roomState == Item::Hotel::kOccupied) tint = sf::Color(255, 200, 0, 90);
						else                                                tint = sf::Color(0, 200, 0, 90);
					}
				} else continue;
			} else if (statusMode == kPric) {
				if (id == "condo" || id == "yoot_condo" || id == "office") {
					if (!item->isOccupied()) {
						tint = sf::Color(255, 200, 0, 110); // yellow = For Sale/Rent
					}
				}
			}
			if (tint.a == 0) continue;

			const int2 vp = item->getPositionPixels();
			const sf::Vector2u vs = item->getSizePixels();
			// Cull offscreen overlays.
			if (!(vp.x + vs.x >= view.position.x) || !(vp.x <= (view.position.x + view.size.x)) ||
			    !((vp.y + vs.y) >= (view.position.y - view.size.y)) || !(vp.y <= view.position.y))
				continue;

			sf::RectangleShape overlay({static_cast<float>(vs.x), static_cast<float>(vs.y)});
			overlay.setPosition({static_cast<float>(vp.x),
			                     static_cast<float>(-(vp.y + static_cast<int>(vs.y)))});
			overlay.setFillColor(tint);
			win.draw(overlay);
			drawnSprites++;
		}
	}

	// Draw active emergency events (fire, bomb alerts) on top of everything
	// so the player can always see and click them.
	win.draw(eventSystem);

	//Highlight the item below the cursor.
	if (!toolPrototype && itemBelowCursor) {
		const int2 itemPixels = itemBelowCursor->getPositionPixels();
		const sf::Vector2u itemSize = itemBelowCursor->getSizePixels();
		sf::RectangleShape s({
			static_cast<float>(itemSize.x),
			static_cast<float>(itemSize.y)
		});
		s.setPosition({
			static_cast<float>(itemPixels.x),
			static_cast<float>(-(itemPixels.y + static_cast<int>(itemSize.y)))
		});
		s.setFillColor(sf::Color(255, 255, 255, 128));
		win.draw(s);
		drawnSprites++;
		if (previousItemBelowCursor != itemBelowCursor) {
			timeWindow.showMessage(itemBelowCursor->prototype->name);
		}
	}

	if (toolPrototype) {
		int height = toolPrototype->size.y;
		if (toolPrototype->icon == ICON_LOBBY) {
			height = lobbyToolHeight();
		}
		sf::RectangleShape placement({
			static_cast<float>(toolPrototype->size.x * 8),
			static_cast<float>(height * 36)
		});
		placement.setPosition({
			static_cast<float>(toolPosition.x * 8),
			static_cast<float>(-(toolPosition.y + height) * 36)
		});
placement.setFillColor(sf::Color(255, 255, 255, 48));
placement.setOutlineColor(sf::Color::White);
placement.setOutlineThickness(1.f);
win.draw(placement);
drawnSprites++;
}

// ISSUE-039: batch drag ghosts — white = buildable & affordable, red = blocked.
if (batchDragActive() && toolPrototype) {
for (const BatchCell & c : getBatchCells()) {
const bool ok = c.valid && batchAllAffordable();
sf::RectangleShape ghost({(float)(toolPrototype->size.x * 8), (float)(toolPrototype->size.y * 36)});
ghost.setPosition({(float)(c.x * 8), (float)(-(c.y + toolPrototype->size.y) * 36)});
ghost.setFillColor(ok ? sf::Color(255, 255, 255, 48) : sf::Color(255, 0, 0, 72));
ghost.setOutlineColor(ok ? sf::Color::White : sf::Color::Red);
ghost.setOutlineThickness(1.f);
win.draw(ghost);
drawnSprites++;
}
}

	//Visualize route.
	glColor3f(0, 1, 0);
	glBegin(GL_LINE_STRIP);
	int prevFloor = 0;
	if (!visualizeRoute.nodes.empty()) prevFloor = visualizeRoute.nodes[0].item->position.y;
	for (std::vector<Route::Node>::iterator nit = visualizeRoute.nodes.begin(); nit != visualizeRoute.nodes.end(); nit++) {
		Route::Node & n = *nit;
		if (n.item != mainLobby) {
			// Use the item's visual X (left edge of its first tile + 4px
			// to centre on the narrow stair/elevator sprite), not the
			// logical-tile centre which is offset by size.x/2 tiles for
			// transport items with large footprints.
			float px = n.item->position.x * 8 + 4;
			glVertex2f(px, -prevFloor * 36 - 5);
			glVertex2f(px, -n.toFloor * 36 - 5);
		}
		prevFloor = n.toFloor;
	}
	glEnd();

	//Play background sounds at a regular interval.
	const double backgroundSoundPeriod = 0.5 * Time::kBaseSpeed;
	if (floor(time.absolute / backgroundSoundPeriod) != floor((time.absolute - time.dta) / backgroundSoundPeriod)) {
		// playRandomBackgroundSound();
	}

	//Adjust pitch of playing sounds.
	for (SoundSet::iterator s = playingSounds.begin(); s != playingSounds.end();) {
		if ((*s)->getStatus() == sf::SoundSource::Status::Stopped) {
			playingSounds.erase(s++);
		} else {
			(*s)->setPitch(1 + (time.speed_animated-1) * 0.2);
			s++;
		}
	}

	//Autorelease sounds.
	for (SoundSet::iterator s = autoreleaseSounds.begin(); s != autoreleaseSounds.end();) {
		if ((*s)->getStatus() == sf::SoundSource::Status::Stopped) {
			delete *s;
			autoreleaseSounds.erase(s++);
		} else {
			s++;
		}
	}

	//Draw the debug string.
	snprintf(debugString, 512, "%i sprites", drawnSprites);
}

void Game::reloadGUI()
{
	// if (mapWindow) {
	// 	mapWindow->RemoveReference();
	// 	mapWindow->Close();
	// }

	// //mapWindow     = gui.loadDocument("map.rml");

	// if (mapWindow)     mapWindow    ->Show();

	toolboxWindow.reload();
	timeWindow.reload();
	mapWindow.reload();
	financeWindow.reload();
	reportDialog.reload();
}

void Game::showReport()
{
	// Rebuild on every open so the rows reflect the quarterly ledger
	// as of right now (settlement time or mid-quarter 'P' press).
	reportDialog.setVisible(true);
	reportDialog.reload();
}

void Game::addItem(Item::Item * item)
{
	assert(item);
	items.insert(item);
	itemsByType[item->prototype->id].insert(item);
	rebuildVenueLists();

	if (item->canHaulPeople()) {
		itemsByType["canHaulPeople"].insert(item);
		if (item->isElevator()) itemsByType["elevator"].insert(item);
		else itemsByType["stairlike"].insert(item);
	}

	if (item->prototype->icon == ICON_FLOOR) {
		// Add floor item
		Item::Floor * f = (Item::Floor *) item;
		if (floorItems.count(item->position.y) != 0) {
			// Replace existing floor item with newly created floor item
			Item::Floor * existing_f = floorItems[item->position.y];
			if (existing_f->size.x >= f->size.x) {
				f->position = existing_f->position;
				f->size = existing_f->size;
			} else {
				std::multiset<int> &existing_f_i = existing_f->interval;
				existing_f_i.erase(existing_f->position.x);
				existing_f_i.erase(existing_f->getRect().maxX());
				existing_f_i.insert(f->position.x);
				existing_f_i.insert(f->getRect().maxX());
			}

			f->interval = existing_f->interval;
			removeItem(existing_f);
		}

		floorItems[item->position.y] = f;
		decorations.updateFloor(item->position.y);
		// A floor appeared (load order or minted slab): relink this level
		// so previously severed YootTower runs reconnect where covered.
		if (activeRules.profile().yootWorld)
			gameMap.revalidateFloorLinks(item->position.y);
	} else {
		// YootTower special footprints (final items rising above the top
		// floor, the subway below the lowest) deliberately have no slab for
		// rows outside the floor limits: construction skips them, and so
		// must the slab minting here (construction calls addItem for the
		// finished item, and so does loading a save).
		const bool yootSpecial = Yoot::usesSpecialFootprint(*this, item->prototype);
		for (int i = 0; i < item->size.y; i++) {
			itemsByFloor[item->position.y + i].insert(item);

			if (floorItems.count(item->position.y + i) == 0) {
				// This is necessary in case of loading a save game where the floor item is not loaded before the building item on that floor.
				if (yootSpecial && !activeRules.profile().allowsCell(item->position.x, item->position.y + i)) {
					LOG(DEBUG, "no floor slab minted under special-footprint %s on floor %i (outside the floor limits)",
					    item->prototype->id.c_str(), item->position.y + i);
					continue;
				}
				int minX = item->position.x;
				int maxX = item->getRect().maxX();
				Item::Floor * f = (Item::Floor *) itemFactory.make(itemFactory.prototypesById["floor"], int2(minX,item->position.y + i));
				f->interval.erase(f->getRect().maxX());
				f->size.x = maxX - minX;
				f->interval.insert(f->getRect().maxX());
				f->updateSprite();
				addItem(f);
			}
			if (!item->canHaulPeople()) {
				FloorItems::iterator fi = floorItems.find(item->position.y + i);
				if (fi != floorItems.end()) {
					std::multiset<int> &interval = fi->second->interval;
					interval.insert(item->position.x);
					interval.insert(item->getRect().maxX());
				} else {
					LOG(WARNING, "unable to find floorItems[%i]", item->position.y + i);
				}
			}
		}
	}

	gameMap.addNode(MapNode::Point(item->position.x + item->size.x/2, item->position.y + item->prototype->entrance_offset), item);
	decorations.updateCrane();
	if (item == metroStation) decorations.updateTracks();
}

void Game::removeItem(Item::Item * item)
{
    if (!item || !items.count(item)) return;
    NameManager::removeTenant(item);
    // Demolition removes an occupied office (6) / condo (3) from CountT.
    if (item->populationCategory() >= 0 && item->populationContribution() != 0)
        addPopulation(item->populationCategory(), -item->populationContribution());
    // Cancel current and queued trips while virtual detach hooks are live.
    for (Person *p : people)
        if (p->journey.references(item)) p->journey.cancel();
    while (!item->people.empty()) (*item->people.begin())->journey.cancel();
    // Owner parking reservations can outlive the parking item.
    if (item->prototype->id == "parking") {
        for (Person *p : people) {
            if (Item::Office::Worker *w = dynamic_cast<Item::Office::Worker *>(p))
                if (w->parkingUsed == item) w->parkingUsed = NULL;
            if (Item::Hotel::Guest *g = dynamic_cast<Item::Hotel::Guest *>(p))
                if (g->parkingUsed == item) g->parkingUsed = NULL;
        }
    }
    // Maids must not keep a target that is about to be deleted.
    if (Item::Hotel *hotel = dynamic_cast<Item::Hotel *>(item))
        for (Item::Item *hk : itemsByType["housekeeping"])
            if (Item::Housekeeping *center = dynamic_cast<Item::Housekeeping *>(hk))
                for (Item::Housekeeping::Maid *maid : center->maidStaff)
                    if (maid && maid->target == hotel) { maid->target = nullptr; if (maid->mode == 1) maid->mode = 0; }
    visualizeRoute.clear();
    inspectorDialog.close();
    elevatorDialog.close();
    if (draggingElevator == item) { draggingElevator = NULL; draggingMotor = 0; }
    if (item->prototype->id != "floor") cancelTransientVip();
    // Cached owner routes also contain raw item pointers.
    for (Item::Item *other : items) {
        for (const Route::Node &node : other->lobbyRoute.nodes)
            if (node.item == item) { other->lobbyRoute.clear(); break; }
        for (const Route::Node &node : other->metroRoute.nodes)
            if (node.item == item) { other->metroRoute.clear(); break; }
    }
    populationNeedsUpdate = true;
	// Drop any pending route rebuild for this item before it is deleted.
	std::vector<Item::Item *>::iterator rq = std::find(routeBuildQueue.begin(), routeBuildQueue.end(), item);
	if (rq != routeBuildQueue.end()) routeBuildQueue.erase(rq);
	items.erase(item);
	itemsByType[item->prototype->id].erase(item);
	rebuildVenueLists();

	if (item->canHaulPeople()) {
		itemsByType["canHaulPeople"].erase(item);
		if (item->isElevator()) itemsByType["elevator"].erase(item);
		else itemsByType["stairlike"].erase(item);
	}

	if (item->prototype->icon == ICON_FLOOR) {
		// Remove floor item
		floorItems.erase(item->position.y);
		decorations.updateFloor(item->position.y);
	} else {
		for (int i = 0; i < item->size.y; i++) {
			itemsByFloor[item->position.y + i].erase(item);
			if (!item->canHaulPeople()) {
                FloorItems::iterator floor = floorItems.find(item->position.y + i);
                if (floor != floorItems.end() && floor->second) {
                    std::multiset<int> &interval = floor->second->interval;
                    auto left = interval.find(item->position.x);
                    if (left != interval.end()) interval.erase(left);
                    auto right = interval.find(item->getRect().maxX());
                    if (right != interval.end()) interval.erase(right);
                }
			}
		}
	}

	eventSystem.people.itemRemoved(item);
	if (item == itemBelowCursor) itemBelowCursor = NULL;
	if (item == mainLobby) mainLobby = NULL;
	if (item == metroStation) metroStation = NULL;

	gameMap.removeNode(MapNode::Point(item->position.x + item->size.x/2, item->position.y + item->prototype->entrance_offset), item);
	decorations.updateCrane();
	if (item->prototype->icon == ICON_METRO) decorations.updateTracks(); // Technically, this should not happen as Metro Stations are not removable.
	delete item;
}

bool Game::extendFloor(int floor, int minX, int maxX, bool free) {
	if (!canExtendFloor(*this, floor, minX, maxX)) {
		LOG(WARNING, "refused floor extension on floor %i outside the %s horizontal build policy",
		    floor, activeRules.displayName());
		return false;
	}
	if (floorItems.count(floor) != 0) {
		// Look for existing floor to extend
		Item::Floor * f = floorItems[floor];
		// YootTower (E2): Floor.t2p charges per cell that had no floor, and
		// a detached run leaves a hole between the two towers.
		const int yootFresh = activeRules.profile().yootWorld ? f->yootCover(minX, maxX) : -1;
		std::multiset<int> &interval = f->interval;
		interval.erase(interval.find(f->position.x));
		interval.erase(interval.find(f->getRect().maxX()));
		gameMap.removeNode(MapNode::Point(f->position.x + f->size.x/2, f->position.y + f->prototype->entrance_offset), f);
		float diff_left = 0;
		if (minX < f->position.x) {
			diff_left = f->position.x - minX;
			f->size.x += diff_left;
			f->setPosition(int2(minX, floor));
		}

		float diff_right = 0;
		diff_right = maxX - f->getRect().maxX();
		if (diff_right < 0) diff_right = 0;
		f->size.x += diff_right;

		f->updateSprite();
		interval.insert(f->position.x);
		interval.insert(f->getRect().maxX());
		gameMap.addNode(MapNode::Point(f->position.x + f->size.x/2, f->position.y + f->prototype->entrance_offset), f);
		if (diff_left + diff_right > 0 || yootFresh > 0) {
			decorations.updateFloor(f->position.y);
			if (!free) {
				transferFunds(yootFresh >= 0 ? -f->prototype->price * yootFresh
				                             : -f->prototype->price * (diff_left + diff_right), "construction", "Floor extension");
				playOnce("simtower/construction/flexible");
			}
		}
	} else {
		// Otherwise construct a new floor
		Item::Floor * f = (Item::Floor *) itemFactory.make(itemFactory.prototypesById["floor"], int2(minX,floor));
		f->interval.erase(f->getRect().maxX());
		f->size.x = maxX - minX;
		f->interval.insert(f->getRect().maxX());
		f->updateSprite();
		addItem(f);
		if (!free) {
			// YootTower: T2TenantDef::CalcPayment charges every empty cell (E2).
			// SimTower: the EXE charges width x price (art batch 6).
			transferFunds(activeRules.profile().yootWorld
			                   ? -Yoot::floorCharge(f->prototype->price, maxX - minX)
			                   : -f->prototype->price * (maxX - minX), "construction", "Floor");
			playOnce("simtower/construction/normal");
		}
	}
	// Floor coverage on this level changed (extended, or holes punched for
	// a detached YootTower run): rebuild the level's horizontal links.
	if (activeRules.profile().yootWorld)
		gameMap.revalidateFloorLinks(floor);
	return true;
}

bool Game::writeTowerFile(const std::string & path)
{
	FILE * f = fopen(path.c_str(), "w");
	if (!f) {
		LOG(ERROR, "Could not open %s for writing", path.c_str());
		return false;
	}
	tinyxml2::XMLPrinter xml;
	encodeXML(xml);
	// Integrity hash, ported from OpenSkyWeb (src/core/savehash.js): the
	// digest covers the exact serialized text minus the hash attribute, so
	// saves interop with OpenSkyWeb's loader (same salt + algorithm).
	const std::string out = SaveHash::injectSaveHash(xml.CStr());
	fwrite(out.data(), 1, out.size(), f);
	fclose(f);
	return true;
}

void Game::autosaveIfDue()
{
	// Policy ported from OpenSkyWeb (src/core/autosave.js): snapshot on an
	// in-game day change, throttled to at most one write per 60 real seconds.
	// Headless runs (--sim) never autosave: the throttle reads the wall clock.
	if (Sim::deterministic()) return;
	const int day = (int)floor(time.absolute);
	if (day == lastAutosaveDay) return;
	const double now = autosaveWallClock();
	if (now - lastAutosaveWallClock < 60.0) return;
	lastAutosaveDay = day;
	lastAutosaveWallClock = now;

	std::string name = saveFilename.empty() ? "default.tower" : saveFilename;
	const std::string suffix = ".tower";
	if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
		name = name.substr(0, name.size() - suffix.size());
	const std::string path = name + ".autosave.tower";
	if (writeTowerFile(path)) {
		LOG(INFO, "autosaved to %s", path.c_str());
		timeWindow.showMessage("Autosaved.");
	}
}

bool Game::repairFloorCoverage()
{
	// Ported from OpenSkyWeb Game::repairFloorCoverage (ISSUE-041): mints
	// free stub floor slabs under hauler columns (stairs/elevators) where a
	// level lost its floor entirely, and restores slab coverage under room
	// footprints. Heals towers saved by older builds on first load.
	std::map<int, std::pair<int,int> > needs; // level -> [minX, maxX)
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		Item::Item * item = *i;
		if (item->prototype->icon == ICON_FLOOR) continue;
		// Loaded geometry is retained verbatim, but repair must never create
		// a slab outside the active ruleset to support it.
		if (!activeRules.allowsHorizontalFootprint(item->position.x, item->size.x)) {
			LOG(WARNING, "skipped floor repair for %s outside the %s horizontal build policy",
			    item->desc().c_str(), activeRules.displayName());
			continue;
		}
		const int h = std::max(1, item->size.y);
		// Same exclusion as addItem/construction: YootTower special
		// footprints stand without slabs outside the floor limits, so the
		// repair must not mint coverage for those rows either.
		const bool yootSpecial = Yoot::usesSpecialFootprint(*this, item->prototype);
		for (int yy = item->position.y; yy < item->position.y + h; yy++) {
			// Haulers only need a slab where the level has none at all;
			// rooms always need coverage spanning their footprint.
			if (item->canHaulPeople() && floorItems.count(yy) != 0) continue;
			if (yootSpecial && !activeRules.profile().allowsCell(item->position.x, yy)) continue;
			const int minX = item->position.x;
			const int maxX = item->getRect().maxX();
			std::map<int, std::pair<int,int> >::iterator n = needs.find(yy);
			if (n == needs.end()) needs[yy] = std::make_pair(minX, maxX);
			else {
				n->second.first = std::min(n->second.first, minX);
				n->second.second = std::max(n->second.second, maxX);
			}
		}
	}
	bool repaired = false;
	for (std::map<int, std::pair<int,int> >::iterator i = needs.begin(); i != needs.end(); i++) {
		const int y = i->first;
		Item::Floor * f = floorItems.count(y) ? floorItems[y] : NULL;
		if (!f || f->position.x > i->second.first || f->getRect().maxX() < i->second.second) {
			if (extendFloor(y, i->second.first, i->second.second, true))
				repaired = true;
		}
	}
	if (repaired) updateRoutes();
	return repaired;
}

void Game::seedOfficeLunchQa()
{
	clearWorld();
	time.set(Time::hourToAbsolute(11.95));
	setSpeedMode(3);
	Item::Lobby *lobby = (Item::Lobby *)itemFactory.make("lobby", int2(-16, 0));
	lobby->size.x = 32;
	addItem(lobby);
	addItem(itemFactory.make("stairs", int2(-4, 0)));
	addItem(itemFactory.make("floor", int2(-16, 1)));
	Item::Office *office = (Item::Office *)itemFactory.make("office", int2(-12, 1));
	addItem(office);
	Item::FastFood *fastFood = (Item::FastFood *)itemFactory.make("fastfood", int2(0, 1));
	fastFood->open = true;
	fastFood->updateSprite();
	addItem(fastFood);
	updateRoutes();
	office->prepareLunchQa(12.0);
	LOG(IMPORTANT, "seeded office lunch QA tower");
}

void Game::seedNewTower()
{
	clearWorld();
	applyNewTowerProfile();
	setSpeedMode(1);
	selectTool("inspector");
	poi.x = 0;
	poi.y = 200;
	updateRoutes();
	LOG(IMPORTANT, "seeded new tower (%s, funds $%lld, %d stars)", activeRules.displayName(), (long long)funds, rating + 1);
}

void Game::applyNewTowerProfile()
{
	const RulesProfile & profile = activeRules.profile();
	// SimTower/OpenSky: the EXE's starting cash (tower_init 10d0:0ac2:
	// 20000 x $100; concilia adoption row 24), kept in lockstep with the
	// balance (the corner-click doubler arms on this exact value).
	// YootTower: WATR initialFunds x ExchangeMoney.
	funds = profile.startFunds;
	money.clear(funds);
	rating = profile.startRating;
	population = 0;
	countT = TenantRules::CountT();
	populationNeedsUpdate = false;
	yootVipFlags = 0;
	yootSecretItems = 0;
	activeRules.setSecretUnlocked(false);
	// YootTower doc init: T2DateTime(1, worldDef+0x74, 7:00).
	if (profile.yootWorld)
		time.set(profile.startDay + Time::hourToAbsolute(7.0));
	else
		time.set(7/78.0);
	lastAccountingDay = (int)floor(time.absolute);
	lastJudgeDay = (int)floor(time.absolute) - (time.hour < 5.0 ? 1 : 0);
	lastAccountingQuarter = (int)floor(time.absolute / 3.0);
	yootRentCycle = lastAccountingQuarter -
		((time.day > 0 || time.hour * 60.0 >= Yoot::kOfficeRentFirstMinute) ? 0 : 1);
	timeWindow.updateFunds();
	timeWindow.updateRating();
}

void Game::setRules(const BuildRules & rules)
{
	activeRules = rules;
	activeRules.setSecretUnlocked((yootSecretItems & 1) != 0);
	const RulesProfile & profile = activeRules.profile();
	Time::setLinearClock(profile.linearClock);
	time.calendar = profile.calendar == RulesProfile::Calendar::YootTower
		? Time::Calendar::YootTower : Time::Calendar::SimTower;
	applyItemCatalog();
	refreshYootArt();
}

void Game::applyItemCatalog()
{
	// Detach and restore first so switching worlds/rulesets never leaves a
	// dangling prototype->yoot or a YootTower price behind.
	static std::map<std::string, int2> baseSizes; // YootTower-only prototypes and hotel rooms
	// Hotel rooms take the TNDF footprint too: Singleroom 4, Twinroom 6,
	// Suiteroom 16, Kegon's Smallroom 10 and Largeroom 16 units wide.
	auto tndfSized = [](const std::string & id) {
		return BuildRules::isYootOnlyPrototype(id) || id.compare(0, 6, "hotel_") == 0;
	};
	for (size_t i = 0; i < itemFactory.prototypes.size(); ++i)
	{
		Item::AbstractPrototype * p = itemFactory.prototypes[i];
		p->yoot = nullptr;
		if (tndfSized(p->id))
		{
			if (!baseSizes.count(p->id)) baseSizes[p->id] = p->size;
			p->size = baseSizes[p->id];
		}
		std::map<std::string, std::pair<int, int> >::const_iterator base = yootBasePrices.find(p->id);
		if (base != yootBasePrices.end())
		{
			p->price = base->second.first;
			p->carCost = base->second.second;
		}
	}
	yootCatalog.clear();
	if (!activeRules.profile().yootWorld || itemFactory.prototypes.empty())
		return;

	// Plan step 4: TNDF/MVDF price, car price and limits from the world's
	// plugins (E1, M1-M5). Prototypes without an available plugin keep
	// their OpenSky numbers.
	yootCatalog = resolveYootCatalog(app.pluginManager, activeRules.profile().worldMask);
	for (size_t i = 0; i < itemFactory.prototypes.size(); ++i)
	{
		Item::AbstractPrototype * p = itemFactory.prototypes[i];
		std::map<std::string, YootItemStats>::const_iterator s = yootCatalog.find(p->id);
		if (s == yootCatalog.end()) continue;
		if (!yootBasePrices.count(p->id))
			yootBasePrices[p->id] = std::make_pair(p->price, p->carCost);
		p->yoot = &s->second;
		p->price = s->second.price;
		// Per-world footprints of the native YootTower items (KRestroom 10x1,
		// KTrashFacility 17x2, RMaintenance 12x1, ...). The gate's 16x2 is
		// not in the TNDF (its width is the lot's), so it keeps its own.
		if (tndfSized(p->id) && s->second.variation == 0 && s->second.width > 0)
			p->size = int2(s->second.width, std::max(1, s->second.height));
		if (s->second.mover && s->second.carPrice > 0)
			p->carCost = s->second.carPrice;
	}
	LOG(INFO, "YootTower item catalog: %zu prototypes overlaid from plugin data", yootCatalog.size());
}

const YootItemStats * Game::yootStats(const Item::Item * item) const
{
	return item ? item->prototype->yoot : nullptr;
}

int Game::yootPriceSelection(const Item::Item * item) const
{
	const YootItemStats * stats = yootStats(item);
	if (!stats) return 0;
	int rentClass = openSkyDefaultRentClass(item->prototype->id);
	// Hotel checkout historically used class 0; YootTower rooms start at the
	// same default selection as every other priced tenant.
	if (rentClass == 0) rentClass = 1;
	if (const Item::Shop * shop = dynamic_cast<const Item::Shop *>(item))
		rentClass = shop->tenant.rentClass;
	// Hotel rooms take their Front Desk's rate for the room type (Front.t2p
	// T2DLG 1000 popups; default selection 3 = index 2).
	const int deskSelection = eventSystem.specials.roomSelection(item);
	if (deskSelection >= 0) return std::min(deskSelection, std::max(0, (int)stats->inMoney.size() - 1));
	return Yoot::selectionFromRentClass(rentClass, (int)stats->inMoney.size());
}

void Game::yootBook(int amount, const char * category, const char * message)
{
	if (amount != 0)
		transferFunds(amount, category, message);
}

const YootWorldArt * Game::yootSkinArt() const
{
	return (skin == Skin::Yoot && yootArt && yootArt->loaded()) ? yootArt.get() : NULL;
}

void Game::refreshYootArt()
{
	if (skin != Skin::Yoot) {
		yootArt.reset();
		yootRecolourPluginViews(std::vector<YootRgb>(), 0, 0);
		return;
	}
	// The YootTower ruleset names its world; other rulesets borrow the
	// world picked in the menu (Tokyo by default).
	YootWorld::Id world = app.selectedWorld();
	const std::string & worldId = activeRules.profile().worldId;
	if (!worldId.empty())
		parseYootWorldIdentifier(worldId, world);
	if (yootArt && yootArt->worldId() == world)
		return;
	yootArt.reset(new YootWorldArt);
	yootArt->load(app.pluginManager, world);
}

int Game::lobbyHeightFromKeys() const
{
	const bool ctrl = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl);
	const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift);
	if (ctrl)
		return shift ? 3 : 2;
	return activeRules.profile().lobbyHeight;
}

/// Grand-lobby height: locked by the first ground-lobby build click; a tower
/// that already has a ground lobby (loaded or imported) is implicitly locked.
int Game::lobbyToolHeight() const
{
	if (lobbyHeightLock > 0) return lobbyHeightLock;
	ItemSetByInt::const_iterator ground = itemsByFloor.find(0);
	if (ground != itemsByFloor.end())
		for (Item::Item * i : ground->second)
			if (i->prototype->icon == ICON_LOBBY) return i->size.y;
	return lobbyHeightFromKeys();
}

/// WP7b: first already-added item whose footprint overlaps a freshly decoded
/// one on a shared floor, when at least one of the two is a type whose
/// footprint was re-sized to the SIMTOWER.EXE values. Floors and people
/// movers are never clashes (they legitimately share cells).
Item::Item * Game::legacyFootprintClash(Item::Item * item)
{
	static const char * const kMigrated[] = {"parking", "hotel_suite", "medicalcenter",
		"parkingramp", "housekeeping", "cathedral"};
	auto migrated = [](const Item::Item * i) {
		for (const char * id : kMigrated) if (i->prototype->id == id) return true;
		return false;
	};
	if (item->prototype->icon == ICON_FLOOR || item->canHaulPeople()) return NULL;
	const int minX = item->position.x, maxX = item->position.x + item->size.x;
	for (int i = 0; i < item->size.y; i++) {
		ItemSetByInt::const_iterator row = itemsByFloor.find(item->position.y + i);
		if (row == itemsByFloor.end()) continue;
		for (Item::Item * other : row->second) {
			if (other->prototype->icon == ICON_FLOOR || other->canHaulPeople()) continue;
			if (!migrated(item) && !migrated(other)) continue;
			if (other->position.x < maxX && other->position.x + other->size.x > minX) return other;
		}
	}
	return NULL;
}

void Game::encodeXML(tinyxml2::XMLPrinter & xml)
{
	xml.OpenElement("tower");
	xml.PushAttribute("accountingCycle", lastAccountingQuarter);
	xml.PushAttribute("funds", std::to_string(funds).c_str());
	xml.PushAttribute("rating", rating);
	if (lobbyHeightLock > 0) xml.PushAttribute("lobbyHeightLock", lobbyHeightLock);
	xml.PushAttribute("time", time.absolute);
	xml.PushAttribute("speed", speedMode);
	xml.PushAttribute("rainy", sky.rainyDay);
	xml.PushAttribute("tool", selectedTool.c_str());
	xml.PushAttribute("ruleset", activeRules.identifier());
	if (activeRules.profile().yootWorld) {
		xml.PushAttribute("world", activeRules.profile().worldId.c_str());
		xml.PushAttribute("yootVipFlags", (unsigned)yootVipFlags);
		xml.PushAttribute("yootRentCycle", yootRentCycle);
		if (yootSecretItems) xml.PushAttribute("yootSecretItems", (unsigned)yootSecretItems);
	}
	xml.PushAttribute("skin", skinIdentifier(skin));


	xml.PushAttribute("x", (int)poi.x);
	xml.PushAttribute("y", (int)poi.y);

	{
		char counts[160];
		snprintf(counts, sizeof(counts), "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			countT.cat[0], countT.cat[1], countT.cat[2], countT.cat[3], countT.cat[4],
			countT.cat[5], countT.cat[6], countT.cat[7], countT.cat[8], countT.cat[9]);
		xml.PushAttribute("countT", counts);
		xml.PushAttribute("countPopulation", countT.population);
		xml.PushAttribute("medicalAdequate", medicalAdequate);
		xml.PushAttribute("recyclingAdequate", recyclingAdequate);
		xml.PushAttribute("recyclingDisplayStage", recyclingDisplayStage);
	}

	// Persist VIP scheduling state so visits don't reset on save/reload.
	vipSystem.encodeXML(xml);

	// Persist event scheduling state (next event time).
	eventSystem.encodeXML(xml);

	xml.OpenElement("money");
	xml.PushAttribute("todayIncome", std::to_string(money.todayIncome).c_str());
	xml.PushAttribute("todayExpenses", std::to_string(money.todayExpenses).c_str());
	xml.PushAttribute("yesterdayIncome", std::to_string(money.yesterdayIncome).c_str());
	xml.PushAttribute("yesterdayExpenses", std::to_string(money.yesterdayExpenses).c_str());
	xml.PushAttribute("lastQuarterBalance", std::to_string(money.lastQuarterBalance).c_str());
	xml.PushAttribute("quarterIncome", std::to_string(money.quarterIncome).c_str());
	xml.PushAttribute("quarterExpenses", std::to_string(money.quarterExpenses).c_str());
	for (std::map<std::string, int64_t>::const_iterator i = money.todayTotalsByCategory.begin(); i != money.todayTotalsByCategory.end(); i++) {
		xml.OpenElement("today");
		xml.PushAttribute("category", i->first.c_str());
		xml.PushAttribute("total", std::to_string(i->second).c_str());
		xml.CloseElement();
	}
	for (std::map<std::string, int64_t>::const_iterator i = money.yesterdayTotalsByCategory.begin(); i != money.yesterdayTotalsByCategory.end(); i++) {
		xml.OpenElement("yesterday");
		xml.PushAttribute("category", i->first.c_str());
		xml.PushAttribute("total", std::to_string(i->second).c_str());
		xml.CloseElement();
	}
	for (std::vector<Money::DaySummary>::const_iterator i = money.recentDays.begin(); i != money.recentDays.end(); i++) {
		xml.OpenElement("recentDay");
		xml.PushAttribute("income", std::to_string(i->income).c_str());
		xml.PushAttribute("expenses", std::to_string(i->expenses).c_str());
		for (std::map<std::string, int64_t>::const_iterator c = i->totalsByCategory.begin(); c != i->totalsByCategory.end(); c++) {
			xml.OpenElement("category");
			xml.PushAttribute("name", c->first.c_str());
			xml.PushAttribute("total", std::to_string(c->second).c_str());
			xml.CloseElement();
		}
		xml.CloseElement();
	}
	for (std::map<std::string, int64_t>::const_iterator i = money.quarterTotalsByCategory.begin(); i != money.quarterTotalsByCategory.end(); i++) {
		xml.OpenElement("quarter");
		xml.PushAttribute("category", i->first.c_str());
		xml.PushAttribute("total", std::to_string(i->second).c_str());
		xml.CloseElement();
	}
	xml.CloseElement();

	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		xml.OpenElement("item");
		(*i)->encodeXML(xml);
		xml.CloseElement();
	}

	xml.CloseElement();
}

static int64_t moneyAttribute(const tinyxml2::XMLElement *e, const char *name, int64_t fallback = 0)
{
	const char *value = e->Attribute(name);
	return value ? std::strtoll(value, NULL, 10) : fallback;
}

void Game::decodeXML(tinyxml2::XMLDocument & xml)
{
	tinyxml2::XMLElement * root = xml.RootElement();
	assert(root);
	BuildRules::Ruleset decodedRuleset = activeRules.ruleset();
	if (!app.hasRulesetOverride()) {
		const char * savedIdentifier = root->Attribute("ruleset");
		if (savedIdentifier) {
			BuildRules::Ruleset savedRuleset = BuildRules::Ruleset::OpenSky;
			if (!BuildRules::parseIdentifier(savedIdentifier, savedRuleset))
				LOG(WARNING, "invalid saved ruleset '%s'; defaulting to opensky", savedIdentifier);
			decodedRuleset = savedRuleset;
		} else if (activeRules.ruleset() == app.selectedRuleset()) {
			// Normal native XML loads begin at the application's default,
			// so old saves safely select OpenSky. A TDT conversion marks
			// its preselected SimTower policy directly before decode.
			decodedRuleset = BuildRules::Ruleset::OpenSky;
		}
	}
	// YootTower world: an explicit --world/menu choice wins, then the saved
	// attribute; saves written before worlds existed load as Tokyo.
	YootWorld::Id decodedWorld = YootWorld::Id::Tokyo;
	if (app.hasWorldOverride()) {
		decodedWorld = app.selectedWorld();
	} else if (const char * savedWorld = root->Attribute("world")) {
		if (!parseYootWorldIdentifier(savedWorld, decodedWorld))
			LOG(WARNING, "invalid saved world '%s'; defaulting to tokyo", savedWorld);
	}
	if (decodedRuleset == BuildRules::Ruleset::YootTower)
		setRules(app.makeBuildRules(decodedRuleset, decodedWorld));
	else if (decodedRuleset != activeRules.ruleset())
		setRules(BuildRules(decodedRuleset));
	else
		setRules(activeRules);


	// Saved skin. An explicit --skin wins; otherwise the persisted
	// identifier re-selects the art set (setSkin no-ops when unchanged,
	// so the setSkin round-trip does not recurse here).
	if (!app.hasSkinOverride()) {
		const char * savedSkin = root->Attribute("skin");
		if (savedSkin) {
			Skin parsed = Skin::OpenSky;
			if (parseSkinIdentifier(savedSkin, parsed)) {
				if (parsed != skin)
					setSkin(parsed);
			} else {
				LOG(WARNING, "invalid saved skin '%s'; keeping current", savedSkin);
			}
		}
	}

	clearWorld();

	setFunds(moneyAttribute(root, "funds"));
	money.clear(funds);
	if (tinyxml2::XMLElement * moneyElement = root->FirstChildElement("money")) {
		money.todayIncome = moneyAttribute(moneyElement, "todayIncome");
		money.todayExpenses = moneyAttribute(moneyElement, "todayExpenses");
		money.yesterdayIncome = moneyAttribute(moneyElement, "yesterdayIncome");
		money.yesterdayExpenses = moneyAttribute(moneyElement, "yesterdayExpenses");
		money.lastQuarterBalance = moneyAttribute(moneyElement, "lastQuarterBalance", money.balance);
		money.quarterStartBalance = money.lastQuarterBalance;
		money.quarterIncome = moneyAttribute(moneyElement, "quarterIncome", 0);
		money.quarterExpenses = moneyAttribute(moneyElement, "quarterExpenses", 0);
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("today"); e; e = e->NextSiblingElement("today")) {
			const char * category = e->Attribute("category");
			if (category) money.todayTotalsByCategory[category] = moneyAttribute(e, "total");
		}
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("yesterday"); e; e = e->NextSiblingElement("yesterday")) {
			const char * category = e->Attribute("category");
			if (category) money.yesterdayTotalsByCategory[category] = moneyAttribute(e, "total");
		}
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("quarter"); e; e = e->NextSiblingElement("quarter")) {
			const char * category = e->Attribute("category");
			if (category) money.quarterTotalsByCategory[category] = moneyAttribute(e, "total");
		}
		for (tinyxml2::XMLElement * e = moneyElement->FirstChildElement("recentDay"); e; e = e->NextSiblingElement("recentDay")) {
			Money::DaySummary day;
			day.income = moneyAttribute(e, "income");
			day.expenses = moneyAttribute(e, "expenses");
			for (tinyxml2::XMLElement * c = e->FirstChildElement("category"); c; c = c->NextSiblingElement("category")) {
				const char * category = c->Attribute("name");
				if (category) day.totalsByCategory[category] = moneyAttribute(c, "total");
			}
			money.recentDays.push_back(day);
		}
	}
	setRating(root->IntAttribute("rating"));
	lobbyHeightLock = root->IntAttribute("lobbyHeightLock", 0);
	if (lobbyHeightLock < 1 || lobbyHeightLock > 3) lobbyHeightLock = 0;
	yootVipFlags = root->UnsignedAttribute("yootVipFlags", 0);
	yootSecretItems = root->UnsignedAttribute("yootSecretItems", 0);
	activeRules.setSecretUnlocked((yootSecretItems & 1) != 0);
	time.set(root->DoubleAttribute("time"));
	time.prev_absolute = time.absolute;
	time.prev_hour = time.hour;
	time.dta = 0;
	lastAccountingDay = (int)floor(time.absolute);
	lastJudgeDay = (int)floor(time.absolute) - (time.hour < 5.0 ? 1 : 0);
	const int currentCycle = (int)floor(time.absolute / 3.0);
	const bool yootClock = activeRules.profile().yootWorld;
	lastAccountingQuarter = root->IntAttribute("accountingCycle",
		currentCycle - (!yootClock && time.hour < 5.0 ? 1 : 0));
	const bool rentDone = time.day > 0 || time.hour * 60.0 >= Yoot::kOfficeRentFirstMinute;
	yootRentCycle = root->IntAttribute("yootRentCycle", currentCycle - (rentDone ? 0 : 1));
	setSpeedMode(root->IntAttribute("speed"));
	sky.rainyDay = root->BoolAttribute("rainy");
	selectTool(root->Attribute("tool"));

	poi.x = root->IntAttribute("x");
	poi.y = root->IntAttribute("y");

	// Restore VIP scheduling state (no-op on older saves - all fields
	// default to fresh-tower values inside decodeXML).
	vipSystem.decodeXML(*root);

	// Restore event scheduling state (no-op on older saves).
	eventSystem.decodeXML(*root);

	const char * savedCountT = root->Attribute("countT");

	tinyxml2::XMLElement * e = root->FirstChildElement("item");
	while (e) {
		Item::Item * item = itemFactory.make(*e);
		// YootTower-only types in a save loaded under another ruleset are
		// rejected by the factory (NULL) and skipped here.
		if (item) {
			// WP7b: footprints changed to the SIMTOWER.EXE values, so an old
			// save can overlap once migrated. Keep the earlier-decoded item,
			// drop this later one and refund its price once.
			if (Item::Item * clash = legacyFootprintClash(item)) {
				const int refund = item->prototype->price;
				LOG(WARNING, "save migration: dropped overlapping '%s' at (%d,%d) (clashes with '%s' at (%d,%d)); refunded $%d",
				    item->prototype->id.c_str(), item->position.x, item->position.y,
				    clash->prototype->id.c_str(), clash->position.x, clash->position.y, refund);
				const long long cap = 9999999900LL;
				money.balance = std::min<long long>(cap, (long long)money.balance + refund);
				setFunds(money.balance);
				delete item;
			} else {
				addItem(item);
			}
		}
		e = e->NextSiblingElement("item");
	}
	if (savedCountT) {
		int v[10] = {0};
		sscanf(savedCountT, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			&v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &v[8], &v[9]);
		countT = TenantRules::CountT();
		int sum = 0;
		for (int c = 0; c < 10; ++c) { countT.cat[c] = v[c]; sum += v[c]; }
		countT.population = root->IntAttribute("countPopulation", sum);
	}

	// Old saves carry no CountT: derive it once from item state.
	if (!savedCountT) recomputeCountT();
	medicalAdequate = root->BoolAttribute("medicalAdequate", true);
	recyclingAdequate = root->BoolAttribute("recyclingAdequate", true);
	recyclingDisplayStage = root->IntAttribute("recyclingDisplayStage", 0);
	rebuildVenueLists();
	vipSystem.relink();
	Item::Parking::relinkAll(this);
	setRating(root->IntAttribute("rating"));
	population = countT.population;
	timeWindow.updatePopulation();
	populationNeedsUpdate = false;
	// Loaded floors decode their holes after the graph links were made;
	// rebuild every level's horizontal links against the real coverage.
	if (activeRules.profile().yootWorld)
		gameMap.revalidateAllFloorLinks();
	// Self-heal missing floor slabs on load (OpenSkyWeb ISSUE-041): repairs
	// towers saved by builds that minted floors with the off-by-one drag bug.
	if (repairFloorCoverage())
		LOG(IMPORTANT, "repairFloorCoverage: minted missing floor slabs on load");
	updateRoutes();
}

void Game::settleDailyAccounting()
{
	// Daily bookkeeping only closes the previous day and refreshes tenant
	// evaluation. Maintenance and recurring rent are quarterly events at
	// 4:59AM, handled by settleQuarterlyAccounting(). The tenant judge runs on
	// the 04:59 settle tick in advanceSimTick (batch 5 T5).
	money.finalizeDay();
	// YootTower tenant evaluation idle (frozen during the blackout); the
	// SimTower ruleset judges at the 04:59 tick in advanceSimTick instead.
	if (activeRules.profile().yootWorld && !eventSystem.tenantIdleFrozen())
		judgeSystem.evaluateAll(this);
	timeWindow.updateMoneyStats();
}

void Game::transferFunds(int64_t f, std::string category, std::string message)
{
	if (f > 0) {
		// EXE clamp 1180:13f2: every credit limits balance+credit to
		// 99,999,999 internal $100 units ($9,999,999,900). Balances are held
		// as 64-bit dollars here so the original ceiling is representable.
		const long long cap = 9999999900LL;
		const long long room = cap - (long long)money.balance;
		f = room <= 0 ? 0 : std::min<long long>((long long)f, room);
	}
	money.record(f, category);
	setFunds(money.balance);
	playOnce("simtower/cash");
	if (!message.empty()) {
		char c[32];
		snprintf(c, 32, ": $%lld", (long long)f);
		timeWindow.showMessage(message + c);
	}
	timeWindow.updateMoneyStats();
}

void Game::setFunds(int64_t f)
{
	if (funds != f) {
		funds = f;
		money.setBalance(funds);
		//timeWindow.updateFunds();
		timeWindow.updateFunds();
		timeWindow.updateMoneyStats();
	}
}

void Game::jumpToHour(double h)
{
	time.set(Time::hourToAbsolute(h));
}

void Game::setRating(int r)
{
	if (rating != r) {
		bool improved = (r > rating);
		rating = r;
		recordSimEvent("rating " + std::to_string(rating));
		if (improved) {
			//TODO: show window
			LOG(IMPORTANT, "rating increased to %i", rating);
			playOnce("simtower/rating/increased");
		}
		timeWindow.updateRating();
	}
}

void Game::addPopulation(int category, int delta)
{
	countT.add(category, delta);
	populationNeedsUpdate = true;
}

void Game::resetCategory(int category)
{
	countT.reset(category);
	populationNeedsUpdate = true;
}

void Game::resetCategoryOnce(int category)
{
	if (countT.resetOnce(category, (int)floor(time.absolute)))
		populationNeedsUpdate = true;
}

void Game::recomputeCountT()
{
	countT = TenantRules::CountT();
	for (ItemSet::const_iterator it = items.begin(); it != items.end(); ++it) {
		Item::Item * item = *it;
		const int cat = item->populationCategory();
		if (cat >= 0) countT.add(cat, item->populationContribution());
	}
	populationNeedsUpdate = true;
}

std::vector<Item::Item *> Game::slotOrder(int floor) const
{
	std::vector<Item::Item *> out;
	ItemSetByInt::const_iterator it = itemsByFloor.find(floor);
	if (it == itemsByFloor.end()) return out;
	for (ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i)
		if ((*i)->prototype->id != "floor") out.push_back(*i);
	std::sort(out.begin(), out.end(), [](const Item::Item * a, const Item::Item * b) {
		if (a->position.x != b->position.x) return a->position.x < b->position.x;
		if (a->prototype->id != b->prototype->id) return a->prototype->id < b->prototype->id;
		return a < b;
	});
	return out;
}

int Game::tenantSlotIndex(const Item::Item * item) const
{
	if (!item) return 0;
	int count = 0;
	ItemSetByInt::const_iterator it = itemsByFloor.find(item->position.y);
	if (it == itemsByFloor.end()) return 0;
	for (ItemSet::const_iterator i = it->second.begin(); i != it->second.end(); ++i) {
		const Item::Item * o = *i;
		if (o == item || o->position.x >= item->position.x) continue;
		const std::string & id = o->prototype->id;
		if (id == "floor" || id == "lobby" || id == "stairs" || id == "escalator" ||
		    o->isElevator() || o->isLobby() || o->isStairlike()) continue;
		++count;
	}
	return count;
}

bool Game::emergencyActive() const
{
	// Fire, bomb threat or disaster (EventSystem::Type != None).
	return eventSystem.isActive();
}

void Game::rebuildVenueLists()
{
	for (auto &zone : venueLists) for (auto &list : zone) list.clear();
	static const char * const ids[4] = {"shop", "restaurant", "fastfood", "medicalcenter"};
	for (int kind = 0; kind < 4; ++kind) {
		ItemSetByString::const_iterator bucket = itemsByType.find(ids[kind]);
		if (bucket == itemsByType.end()) continue;
		std::vector<Item::Item *> sorted(bucket->second.begin(), bucket->second.end());
		std::sort(sorted.begin(), sorted.end(), [](const Item::Item *a, const Item::Item *b) {
			if (a->position.y != b->position.y) return a->position.y < b->position.y;
			if (a->position.x != b->position.x) return a->position.x < b->position.x;
			return a < b;
		});
		for (Item::Item *item : sorted) {
			const int f = item->position.y + 5;
			const int z = f / 15;            // trunc toward zero (EXE band)
			if (z < 0 || z > 6 || f % 15 > 9) continue; // negative remainders pass
			venueLists[z][kind].push_back(item);
		}
	}
}

Item::Item * Game::pickVenue(int originFloor, int kind)
{
	if (kind < 0 || kind > 3) return NULL;
	int z = (originFloor + 1) / 15;
	if (z < 0) z = 0;
	if (z > 6) z = 6;
	std::vector<Item::Item *> *list = &venueLists[z][kind];
	if (list->empty()) list = &venueLists[0][kind];
	if (list->empty()) return NULL;
	Item::Item *item = (*list)[rand() % list->size()];
	return items.count(item) ? item : NULL;
}

Item::Item * Game::pickMedicalCenter(int originFloor)
{
	Item::Item *center = pickVenue(originFloor, 3);
	if (!center) medicalAdequate = false; // sick worker found no center
	return center;
}

void Game::setPopulation(int p)
{
	if (population != p) {
		population = p;
		ratingMayIncrease();
		timeWindow.updatePopulation();
	}
}

/** Called whenever an event occurs that might allow the rating to increase, e.g. a change in
 *  population, or an item constructed. */
void Game::ratingMayIncrease()
{
	// YootTower worlds promote on population + VIP flag only (no facility
	// scan, no time window); see yootCheckGrade.
	if (!activeRules.profile().grades.empty())
	{
		yootCheckGrade();
		return;
	}

	// Run a fresh judge pass so facility counts reflect the current state
	// (e.g. the Security office the player just finished). This is cheap
	// relative to construction events.
	judgeSystem.evaluateAll(this);
	const JudgeSystem::Counts & counts = judgeSystem.counts();

	// Facility scan the counts struct cannot answer: hotel suites, recycling
	// centers and the 15-floor medical coverage (concilia LevelUp 1148:0000 /
	// scan_promotion_flags). The gate itself is pure and lives in LevelUp.
	LevelUp::PromotionContext ctx;
	ctx.population = population;
	ctx.securityOffices = counts.securityOffices;
	ctx.metros = counts.metros;
	Game::ItemSetByString::const_iterator suites = itemsByType.find("hotel_suite");
	ctx.hotelSuites = (suites == itemsByType.end()) ? 0 : (int)suites->second.size();
	// Adequacy flags are written by the scheduled recycling stages and the
	// sick-worker lookup (T33/T42); with no completed center there is none.
	int recyclingCenters = 0, medicalCenters = 0;
	for (ItemSet::const_iterator i = items.begin(); i != items.end(); ++i)
	{
		if ((*i)->underConstruction) continue;
		const std::string & id = (*i)->prototype->id;
		if (id == "recycling") ++recyclingCenters;
		else if (id == "medicalcenter") ++medicalCenters;
	}
	ctx.recyclingAdequate = recyclingAdequate && recyclingCenters > 0;
	ctx.medicalAdequate = medicalAdequate && medicalCenters > 0;
	ctx.vipReviews = vipSystem.positiveReviews();
	ctx.hour = time.hour;
	ctx.day = time.day;
	// TOWER latch: the 5-star -> TOWER row only opens once a cathedral
	// wedding has been held. Cathedral::startWedding re-runs this gate
	// mid-ceremony; the EXE never promotes from 5 stars in LevelUp
	// (1150:014d-0154), so population alone must not crown the tower.
	ctx.weddingCompleted = Item::Cathedral::weddingCompleted(this);

	while (true)
	{
		const LevelUp::Requirements * req = LevelUp::advancementRequirements(rating);
		if (!req) break; // already at max rating

		if (!LevelUp::meetsRequirements(*req, ctx))
		{
			// Surface what's missing as a one-shot hint, but only when the
			// player is close enough that the next push might trigger it.
			if (population >= req->population - 50 && population < req->population)
			{
				timeWindow.showMessage(std::string("Next: ") + req->summary);
			}
			break;
		}
		setRating(rating + 1);
		// Promotion ceremony (concilia LevelUp 1148:004f and @0047-004a):
		// every promotion consumes the favorable-VIP verdict and re-seeds
		// the buried-treasure latch with the new tier's dig condition.
		vipSystem.clearApproval();
		eventSystem.armTreasureLatch(rating + 1);
		// Show a level-up modal in addition to the transient message.
		// The modal lists the freshly unlocked items so the player knows
		// what they can now build (Phase 3.2).
		levelUpDialog.showForRating(rating);
		timeWindow.showMessage(std::string("Promoted to ") + std::to_string(rating + 1) + " stars!");
		// No star award gift: the errata's promotion bonus was the buried-
		// treasure payout misfiled (Phase 1a adjudication). LevelUp keeps the
		// starRewardMessage API for callers, but it always returns empty.
		toolboxWindow.reload(); // refresh build locks
	}
}

void Game::yootCheckGrade()
{
	const std::vector<RulesProfile::Grade> & grades = activeRules.profile().grades;
	while (true)
	{
		// Row grade-1 gates the step from `grade` (CheckGameLevelProc
		// 0x10072410 -> 0x2715 -> T2.exe FUN_00421e90 grade++).
		const int grade = rating + 1;
		if (grade < 1 || grade > (int)grades.size()) break;
		const RulesProfile::Grade & row = grades[grade - 1];
		if (!LevelUp::yootPromotionReady(row.vipFlag, row.population, yootVipFlags, population))
			break;
		setRating(rating + 1);
		levelUpDialog.showForRating(rating);
		// The world's GetGradeUpMessage (Tokyo / Kegon override, Hawaii base).
		YootWorld::Id world = YootWorld::Id::Tokyo;
		parseYootWorldIdentifier(activeRules.profile().worldId, world);
		timeWindow.showMessage(yootGradeUpMessage(world, rating + 1));
		toolboxWindow.reload(); // refresh build locks
	}
}

bool Game::grantFinalGrade()
{
	const std::vector<RulesProfile::Grade> & grades = activeRules.profile().grades;
	const int grade = rating + 1;
	if (grade < 1 || grade > (int)grades.size()) return false;
	if (!LevelUp::yootGrantFinalGrade(grade, (int)grades.size(), grades[grade - 1].vipFlag, yootVipFlags))
		return false;
	LOG(IMPORTANT, "final-item event granted grade %d flag 0x%x", grade, (unsigned)grades[grade - 1].vipFlag);
	yootCheckGrade();
	return true;
}

void Game::setSpeedMode(int sm)
{
	// Clamp instead of asserting: saves may carry a speed written by a
	// different version (Wave U added 4 = Fast; legacy saves are 0..3).
	if (sm < 0) sm = 0;
	if (sm > kFastSpeedMode) sm = kFastSpeedMode;
	if (speedMode != sm) {
		speedMode = sm;
		double speed = 0;
			switch (speedMode) {
				case 0: speed = 0; break;
				case 1: speed = 1; break;
				case 2: speed = 4; break;
				case 3: speed = 12; break;
				case 4: speed = kFastSpeed; break;
			}
		time.speed = speed;
		// Fast skips the animation ramp: full pace from the first frame
		// (Wave U row 51: no per-frame animation pacing).
		if (speedMode == kFastSpeedMode) time.speed_animated = speed;
		toolboxWindow.updateSpeed();
	}
}

void Game::cycleStatusMode()
{
	StatusMode next = static_cast<StatusMode>((static_cast<int>(statusMode) + 1) % 5);
	if (next == kOutside && skin != Skin::Yoot) next = kNormal;
	statusMode = next;
	const char *names[] = { "Normal view", "Evaluation view", "Pricing view", "Hotel view", "Outside view" };
	timeWindow.showMessage(names[static_cast<int>(next)]);
}

/// Live art-set switch. The cache purge forces every texture request
/// through the new skin's provider chain; items are rebuilt through the
/// encode→decode round-trip so their init()-time sprite setup re-runs
/// against the new art while all persisted state survives.
void Game::setSkin(Skin newSkin)
{
	if (newSkin == skin)
		return;

	if (!app.hasSkinAssets(newSkin)) {
		LOG(WARNING, "Skin '%s' assets are unavailable; staying on %s art",
		    skinDisplayName(newSkin), skinDisplayName(skin));
		return;
	}

	// Drop every cached texture BEFORE re-installing EXE sheets, so the
	// previous skin's provider results are gone and the new gate decides
	// what gets installed. The skin must be active BEFORE the loader runs:
	// the EXE-sheet install gate reads BitmapManager's skin.
	app.bitmaps.clear();
	app.bitmaps.setSkin(newSkin);

	if (newSkin == Skin::Classic) {
		SimTowerLoader loader(&app);
		if (!loader.load()) {
			LOG(WARNING, "Classic skin requires SIMTOWER.EXE; staying on OpenSky art");
			newSkin = Skin::OpenSky;
			app.bitmaps.clear();
			app.bitmaps.setSkin(newSkin);
		}
	}
	skin = newSkin;
	refreshYootArt();

	// Rebuild items in place: serialize the world, clear it, reload. The
	// item decode path re-runs init() with the same state, so sprites are
	// recreated against the new textures.
	tinyxml2::XMLPrinter printer;
	encodeXML(printer);
	tinyxml2::XMLDocument doc;
	if (doc.Parse(printer.CStr(), printer.CStrSize() - 1) == tinyxml2::XML_SUCCESS) {
		decodeXML(doc);
	} else {
		LOG(ERROR, "setSkin: XML round-trip failed (%s)", doc.ErrorStr());
	}

	reloadGUI();
	decorations.reloadTextures();
	timeWindow.showMessage(std::string("Skin: ") + skinDisplayName(skin));
}

void Game::cycleSkin()
{
	// Cycle within the mode's sanctioned edition set (review matrix;
	// OpenSky mode is unrestricted and offers all three).
	const std::size_t count = sanctionedSkinCount(activeRules.ruleset());
	std::size_t next = 0;
	for (std::size_t i = 0; i < count; i++) {
		if (sanctionedSkin(activeRules.ruleset(), i) == skin)
			next = (i + 1) % count;
	}
	setSkin(sanctionedSkin(activeRules.ruleset(), next));
}

void Game::centerViewportOnTile(double tileX, double tileY)
{
	poi.x = tileX * 8.0;
	poi.y = tileY * 36.0;
}

void Game::zoomBy(double factor, const sf::Vector2i * anchorPx)
{
	const sf::Vector2u ws = app.window.getSize();
	const double newZoom = std::clamp(zoom * factor, 1.0 / 3.0, 2.0);
	if (newZoom == zoom)
		return;
	const double f = newZoom / zoom;
	const double halfx = ws.x * 0.5 * zoom,  halfy = ws.y * 0.5 * zoom;
	const double nhalfx = halfx * f,          nhalfy = halfy * f;
	const double ax = anchorPx ? anchorPx->x : ws.x * 0.5;
	const double ay = anchorPx ? anchorPx->y : ws.y * 0.5;
	// Camera view rect in SFML view space (y grows down):
	//   position = (poi.x - halfx, -(poi.y + halfy)), size = 2 * halfsize.
	// Keep the world point under the anchor fixed while swapping the rect
	// for the zoomed one, then read the new poi back off the new rect.
	const double vx = poi.x - halfx, vy = -(poi.y + halfy);
	const double wx = vx + ax * (2 * halfx) / ws.x;
	const double wy = vy + ay * (2 * halfy) / ws.y;
	poi.x = (wx - ax * (2 * nhalfx) / ws.x) + nhalfx;
	poi.y = -(wy - ay * (2 * nhalfy) / ws.y) - nhalfy;
	zoom = newZoom;
}

void Game::toggleElevatorService(Item::Elevator::Elevator * e, int floor)
{
	if (!e) return;
	if (e->unservicedFloors.count(floor) && !e->canAddStop())
	{
		timeWindow.showMessage("This elevator cannot stop at any more floors.");
		return;
	}
	if (floor < e->position.y || floor >= e->position.y + e->size.y) return;
	// Disabled valid stops must remain toggleable; reject structural stops
	// using the same predicate as Express::connectsFloor before any mutation.
	if (e->isExpress() && floor > 0 && !Item::Lobby::isSkyLobbyFloor(floor)) return;	if (!e->unservicedFloors.erase(floor))
	{
		e->unservicedFloors.insert(floor);
		gameMap.removeNode(MapNode::Point(e->position.x + e->size.x / 2, floor), e);
	}
	else if (e->connectsFloor(floor))
	{
		gameMap.addNode(MapNode::Point(e->position.x + e->size.x / 2, floor), e);
	}
	e->cleanQueues();
	updateRoutes();
}

// Number of deck cells that extendFloor() would newly create on `floor` for
// the span [minX, maxX): the whole width of an empty floor, otherwise the
// growth beyond the existing left/right edges.
static int newFloorCells(const Game &game, int floor, int minX, int maxX)
{
	Game::FloorItems::const_iterator it = game.floorItems.find(floor);
	if (it == game.floorItems.end()) return std::max(0, maxX - minX);
	const Item::Item *f = it->second;
	const int left = std::max(0, f->position.x - minX);
	const int right = std::max(0, maxX - f->getRect().maxX());
	return left + right;
}

// True when an express shaft already stands in this column directly above or
// below `floor` (segments that touch an existing express column are
// extensions and may sit on any floor).
static bool touchesExpressColumn(const Game &game, int x, int floor)
{
	Game::ItemSetByString::const_iterator group = game.itemsByType.find("elevator-express");
	if (group == game.itemsByType.end()) return false;
	for (Item::Item *e : group->second) {
		if (e->position.x != x) continue;
		for (int df = -1; df <= 1; df += 2) {
			const int f = floor + df;
			if (f >= e->position.y && f < e->position.y + e->size.y) return true;
		}
	}
	return false;
}

bool Game::placementRuleBlock(Item::AbstractPrototype * proto, int x, int y, std::string & reason) const
{
	const std::string & id = proto->id;
	const RulesProfile & profile = activeRules.profile();
	const bool yoot = profile.yootWorld;
	const bool isFloor = proto->icon == ICON_FLOOR;
	const bool isMetro = proto->icon == ICON_METRO;
	const bool isElevator = id.find("elevator") == 0;
	const int top = y + proto->size.y - 1;
	auto count = [this](const char * key) {
		ItemSetByString::const_iterator it = itemsByType.find(key);
		return it == itemsByType.end() ? 0 : (int)it->second.size();
	};
	auto fail = [&reason](const char * text) { reason = text; return true; };

	// B1: build ceiling F100 (the cathedral spire may rise to 104), basements to B10.
	if (yoot) {
		if (y < profile.lowestFloor) {
			reason = "Cannot build below floor B" + std::to_string(-profile.lowestFloor);
			return true;
		}
	} else if (y < -10 || top > (id == "cathedral" ? 104 : 100)) {
		return fail("Maximum height has been reached");
	}
	if (isElevator && y <= -10) return fail("Elevators cannot go below B9");
	if (isFloor) return false;

	// B2: cathedral only on floor 100, and only one.
	if (id == "cathedral") {
		if (!yoot && y != 100) return fail("Cathedral is available only on 100th floor");
		if (count("cathedral") > 0) return fail("Only one Cathedral allowed");
	}

	// B3: underground-only and not-underground item classes.
	if ((isMetro || id == "parking" || id == "parkingramp" || id == "recycling") && top >= 0)
		return fail("Item unavailable above ground");
	if ((id == "office" || id == "condo" || id == "yoot_condo" || id == "hotel_single" ||
	     id == "hotel_double" || id == "hotel_suite") && y < 0)
		return fail("Item not available underground");

	// B5: count caps.
	if (isMetro && metroStation != NULL) return fail("Only one Metro Station allowed");
	if ((id == "cinema" || id == "partyhall") && count("cinema") + count("partyhall") >= 16)
		return fail("Item no longer available");
	if ((id == "restaurant" || id == "shop" || id == "fastfood") &&
	    count("restaurant") + count("shop") + count("fastfood") >= 512)
		return fail("Item no longer available");
	if (id == "medicalcenter" && count("medicalcenter") >= 10) return fail("Item no longer available");
	if (id == "security" && count("security") >= 10) return fail("Item no longer available");
	if (id == "parking" && count("parking") >= 512) return fail("Item no longer available");

	// B3: a recycling center after the first must touch an existing one.
	if (id == "recycling" && count("recycling") > 0) {
		bool adjacent = false;
		const int w = proto->size.x;
		for (Item::Item * r : itemsByType.find("recycling")->second) {
			// Concilia envelope: floors -2..+1 around the new base, within 25 cells.
			const bool floors = r->position.y >= y - 2 && r->position.y <= y + 1;
			if (floors && r->position.x + r->size.x - 1 >= x - 25 && r->position.x <= x + w - 1 + 25) { adjacent = true; break; }
		}
		if (!adjacent) return fail("Must be placed near an existing Recycling Center");
	}

	// B4: the metro sits on the bottom floor; nothing at or below it.
	if (metroStation != NULL && !isMetro && y <= metroStation->position.y)
		return fail("Cannot place items under Metro");
	if (isMetro) {
		for (ItemSetByInt::const_iterator fl = itemsByFloor.begin(); fl != itemsByFloor.end(); ++fl) {
			if (fl->first > y) break;
			for (Item::Item * other : fl->second)
				if (other->prototype->icon != ICON_FLOOR) return fail("Place Metro station on bottom floor");
		}
	}

	// B6: parking bays need a ramp on their floor; ramps chain down from B1.
	if (id == "parking") {
		bool ramp = false;
		ItemSetByString::const_iterator rs = itemsByType.find("parkingramp");
		if (rs != itemsByType.end())
			for (Item::Item * r : rs->second) if (r->position.y == y) { ramp = true; break; }
		if (!ramp) return fail("Parking Ramps must be placed on this level");
	}
	if (id == "parkingramp") {
		ItemSetByString::const_iterator rs = itemsByType.find("parkingramp");
		bool linked = y == -1;
		if (rs != itemsByType.end()) {
			for (Item::Item * r : rs->second) {
				if (r->position.y == y) return fail("This floor already has a Parking Ramp");
				if (r->position.x != x) return fail("Parking Ramps must be connected vertically");
				if (r->position.y == y + 1) linked = true; // chain runs down from B1
			}
		}
		if (!linked) return fail("Parking Ramps must connect to the 1st floor");
	}
	return false;
}

bool Game::bulldozeItem(Item::Item * item)
{
	if (!item || !items.count(item)) return false;
	const std::string & id = item->prototype->id;
	const int icon = item->prototype->icon;
	// Indestructible: every lobby, the deck, security, housekeeping, metro, cathedral.
	if (icon == ICON_LOBBY || icon == ICON_FLOOR || icon == ICON_METRO ||
	    id == "security" || id == "housekeeping" || id == "cathedral") {
		playOnce("simtower/construction/impossible");
		timeWindow.showMessage("Cannot destroy this item");
		return false;
	}
	if (item->underConstruction) {
		playOnce("simtower/construction/impossible");
		timeWindow.showMessage("Cannot destroy items under construction");
		return false;
	}
	LOG(DEBUG, "destroy %s", item->desc().c_str());
	// Demolishing an occupied condo buys the owner out at the class table value.
	const int buyout = item->demolitionBuyout();
	if (buyout > 0) transferFunds(-buyout, "condo_buyback", "Condo owner buyout");
	const bool canHaulPeople = item->canHaulPeople();
	removeItem(item);
	if (canHaulPeople) updateRoutes();
	playOnce("simtower/bulldozer");
	return true;
}

bool Game::constructToolAt(int2 pos)
{
    if (rejectDisasterWorldEdit()) return false;
	toolPosition = pos;
	// The first ground-lobby build click locks the grand-lobby height for the
	// tower (plain = profile default, Ctrl = 2, Ctrl+Shift = 3), even when
	// the placement below fails; later clicks ignore the modifiers.
	if (toolPrototype->icon == ICON_LOBBY && pos.y == 0 && lobbyHeightLock == 0)
		lobbyHeightLock = lobbyToolHeight();
	const int lobbyHeightNow = pos.y == 0 ? lobbyToolHeight() : lobbyHeightFromKeys();
	const int footprintHeight = toolPrototype->icon == ICON_LOBBY ? lobbyHeightNow : toolPrototype->size.y;
    if (eventSystem.rubbleIntersects(recti(pos, int2(toolPrototype->size.x, footprintHeight)))) {
        playOnce("simtower/construction/impossible");
        timeWindow.showMessage("Bulldoze the burned area before rebuilding.");
        return false;
    }
	const bool cornerDoubler = pos == int2(-187, -10) && items.empty() && funds == 2000000;
	if (!cornerDoubler) {
		std::string ruleReason;
		if (placementRuleBlock(toolPrototype, pos.x, pos.y, ruleReason)) {
			playOnce("simtower/construction/impossible");
			timeWindow.showMessage(ruleReason);
			return false;
		}
	}
	// YootTower final items and the subway leave the floor area (step 13).
	const bool yootSpecial = Yoot::usesSpecialFootprint(*this, toolPrototype);
	if (!(yootSpecial ? activeRules.allowsSpecialFootprint(pos.x, pos.y, toolPrototype->size.x, footprintHeight)
	                  : activeRules.allowsFootprint(pos.x, pos.y, toolPrototype->size.x, footprintHeight))) {		const std::string reason = "Footprint is outside the " +
			std::string(activeRules.displayName()) + " build boundary";
		LOG(DEBUG, "cannot construct %s at %ix%i: %s",
		    toolPrototype->id.c_str(), pos.x, pos.y, reason.c_str());
		playOnce("simtower/construction/impossible");
		timeWindow.showMessage("Cannot place item there. " + reason + ".");
		return false;
	}


	// Corner-click money doubler (EXE build-dispatcher prologue,
	// 11f8:0955; concilia adoption row 24): a build-tool click on the
	// lot's bottom-left cell — B10 (floor -10), lot cell 0 (our x = -187,
	// TDT X_ORIGIN) — of a virgin tower (zero items, so the lobby-height
	// choice is still open) holding EXACTLY the starting $2,000,000
	// awards the current balance, a silent one-shot doubling to $4M. The
	// click is consumed and nothing is placed; any spend or placement
	// breaks the exact-balance/virgin conditions and disarms it forever.
	if (pos == int2(-187, -10) && items.empty() && funds == 2000000) {
		transferFunds(funds, "easter_egg", "Corner-click windfall");
		LOG(IMPORTANT, "corner-click doubler: funds doubled to %lld", (long long)funds);
		return true; // click consumed (no item built)
	}
	const size_t itemsBefore = items.size();
	const int fundsBefore = funds;
	if (toolPrototype->id == "skybridge" && activeRules.profile().yootWorld)
		return Yoot::constructSkyBridge(*this, toolPrototype, toolPosition);
	int height = toolPrototype->size.y;
	int yHeightOffset = height;
	if (toolPrototype->icon == ICON_LOBBY) {
		height = lobbyHeightNow;
		yHeightOffset = 1;
	}
	struct SizeGuard {
		int &val;
		int oldVal;
		~SizeGuard() { val = oldVal; }
	} guard{toolPrototype->size.y, toolPrototype->size.y};

	if (toolPrototype->icon == ICON_LOBBY) {
		toolPrototype->size.y = height;
	}

	bool handled = false;
	recti toolBoundary = recti(toolPosition, toolPrototype->size);
	if (toolPrototype->id.find("elevator") == 0) {
		const ItemSet &elevators = itemsByType[toolPrototype->id];
		for (ItemSet::const_iterator i = elevators.begin(); i != elevators.end(); i++) {
			Item::Elevator::Elevator * e = (Item::Elevator::Elevator *) *i;
			recti itemRect = e->getRect();
			if (toolBoundary.minX() == itemRect.minX() &&
				toolBoundary.maxX() == itemRect.maxX() &&
				toolBoundary.minY() >= itemRect.minY() &&
				toolBoundary.maxY() <= itemRect.maxY()) {
				if (!e->canAddCar(toolPosition.y)) {
					timeWindow.showMessage("No more cars in this shaft.");
					playOnce("simtower/construction/impossible");
				} else if (money.balance < toolPrototype->carCost) {
					timeWindow.showMessage("Not enough money for an elevator car.");
					playOnce("simtower/construction/impossible");
				} else {
					LOG(DEBUG, "add car on floor %i to elevator %s", toolPosition.y, e->desc().c_str());
					e->addCar(toolPosition.y);
					transferFunds(-toolPrototype->carCost, "construction", "Elevator car");
				}
				handled = true;
				break;
			}
		}
	}
if (!handled) {
	bool constructionBlocked = false;
	std::string blockReason;
	std::string moneyReject; // exact EXE message 7/8 text when unaffordable
	int minFloorX = INT_MAX;
	int maxFloorX = INT_MIN;
	for (int i = 0; i < toolPrototype->size.y; ++i) {
		const int f = toolPosition.y + i;
		if (yootSpecial && (f < activeRules.profile().lowestFloor || f > activeRules.profile().highestFloor))
			continue; // no slab outside the floor area (addItem fills in)
		if (!canExtendFloor(*this, toolPosition.y + i, toolPosition.x,
		                    toolPosition.x + toolPrototype->size.x)) {
			constructionBlocked = true;
			blockReason = "Horizontal footprint is outside the " +
				std::string(activeRules.displayName()) + " build boundary";
			break;
		}
	}


	// Star-rating gate: some prototypes require a minimum tower
	// rating to build. Reject early so the player gets feedback.
	// Elevator placement gates belong to the construction path as well as
	// the UI: 24 groups, express sky-lobby anchoring, metro depth, and
	// grand-lobby upper-story exclusion are all checked before charging.
	if (toolPrototype->id.find("elevator") == 0) {
		const bool express = toolPrototype->icon == 6;
		if (itemsByType["elevator"].size() >= Item::Elevator::Elevator::kMaxShafts) {
			constructionBlocked = true;
			blockReason = "No more elevator shafts available";
		} else if (express && toolPosition.y > 0 && !Item::Lobby::isSkyLobbyFloor(toolPosition.y) &&
		           !touchesExpressColumn(*this, toolPosition.x, toolPosition.y)) {
			constructionBlocked = true;
			blockReason = "New express shafts anchor at lobby floors";
		} else if (mainLobby && toolPosition.y >= mainLobby->position.y + 1 &&
		           toolPosition.y < mainLobby->position.y + mainLobby->size.y) {
			constructionBlocked = true;
			blockReason = "Cannot start a shaft inside the lobby's upper stories";
		}
	}
	int minRating = activeRules.minRatingToBuild(toolPrototype->id);
	if (minRating == BuildRules::kUnavailable)
	{
		constructionBlocked = true;
		blockReason = toolPrototype->name + " is not available in " + activeRules.profile().worldName;
	}
	else if (minRating > rating)
	{
		constructionBlocked = true;
		blockReason = toolPrototype->name + " unlocks at " +
		              std::to_string(minRating + 1) + " stars";
	}
	else if (toolPrototype->yoot && toolPrototype->yoot->maxBuild > 0 &&
	         (int)itemsByType[toolPrototype->id].size() >= toolPrototype->yoot->maxBuild)
	{
		// YootTower TenantDef maxBuildCount (W6, T2.exe STRTABLE 9999).
		constructionBlocked = true;
		blockReason = toolPrototype->name + " can only be placed once";
	}
	if (!constructionBlocked) {
		// YootTower L1/L2/M6/I7 (top floor, subway, lobby, parking).
		blockReason = Yoot::placementError(*this, toolPrototype, toolPosition.x, toolPosition.y, footprintHeight);
		constructionBlocked = !blockReason.empty();
	}

	const int lowestFloor = activeRules.profile().lowestFloor;
	// The YootTower subway sits below the lowest floor (placementError).
	if (toolPosition.y < lowestFloor && toolPrototype->icon != ICON_METRO) {
			constructionBlocked = true;
			blockReason = "Cannot build below floor B" + std::to_string(-lowestFloor);
		}

		if (metroStation != NULL) {
			const bool elevatorTool = toolPrototype->id.find("elevator") == 0;
			const int lowestAllowed = metroStation->position.y -
				(elevatorTool ? Item::Elevator::Elevator::kMetroClearanceFloors : 0);
			if (toolPosition.y < lowestAllowed) {
				constructionBlocked = true;
				blockReason = "Cannot build below Metro Station";
			}
		}

		if (toolPosition.y > 0 && toolPrototype->icon == ICON_METRO) {
			constructionBlocked = true;
			blockReason = toolPrototype->name + " unavailable above ground";
		}

		// (Cinema above ground is allowed per the EXE rules; the YootTower
		// Theater replaces cinema there, so no preview rule is needed.)
		if (toolPrototype->icon == ICON_LOBBY) {
			if (toolPosition.y != 0 && !Item::Lobby::isSkyLobbyFloor(toolPosition.y)) {
				constructionBlocked = true;
				blockReason = "Lobbies can only be built on every 15th floor";
			}
			else if (toolPosition.y != 0) {
				// Check floor width below
				if (floorItems.count(toolPosition.y - 1) != 0) {
					Item::Item * i = floorItems[toolPosition.y - 1];
					minFloorX = i->position.x;
					maxFloorX = i->getRect().maxX();
				}

				// Check for non-lobby items blocking construction on lobby floor
				ItemSet itemsOnFloor = itemsByFloor[toolPosition.y];
				for (ItemSet::const_iterator ii = itemsOnFloor.begin(); !constructionBlocked && ii != itemsOnFloor.end(); ii++) {
					Item::Item * i = *ii;
					if (i->canHaulPeople() || i->prototype->icon == ICON_LOBBY) continue;
					minFloorX = std::max(minFloorX, i->getRect().maxX());
					maxFloorX = std::min(maxFloorX, i->position.x);
				}
			} else {
				minFloorX = INT_MIN;
				maxFloorX = INT_MAX;
			}
		} else if (toolPrototype->id == "stairs" || toolPrototype->id == "escalator") {
			const bool escalator = toolPrototype->id == "escalator";
			if (itemsByType["stairlike"].size() >= 64) {
				constructionBlocked = true;
				blockReason = escalator ? "No more escalators available" : "No more stairs available";
			}
			if (escalator && !constructionBlocked) {
				for (int landing = 0; landing < 2; ++landing) {
					const int y = toolPosition.y + landing;
					const int x = toolPosition.x + (landing ? 7 : 0);
					bool allowed = false;
					auto deck = floorItems.find(y);
					if (deck != floorItems.end() && !deck->second->underConstruction &&
					    x >= deck->second->position.x && x < deck->second->getRect().maxX()) allowed = true;
					for (auto *tenant : itemsByFloor[y]) {
						if (tenant->prototype->id == "floor" || tenant->canHaulPeople() ||
						    !tenant->getRect().containsPoint(int2(x, y))) continue;
						const std::string &id = tenant->prototype->id;
						allowed = !tenant->underConstruction && (id == "restaurant" || id == "shop" ||
						          id == "fastfood" || id == "cinema" || id == "lobby" ||
						          id == "partyhall" || id == "metro");
						if (!allowed) break;
					}
					if (!allowed) {
						constructionBlocked = true;
						blockReason = "Escalators available only at commercial spaces";
						break;
					}
				}
			}
			// Check obstruction from other transport items
			const ItemSet &stairlike = itemsByType["stairlike"];
			for (ItemSet::const_iterator ii = stairlike.begin(); !constructionBlocked && ii != stairlike.end(); ii++) {
				Item::Item * i = *ii;
				int xOffset = (toolPosition.x - i->position.x);
				if ((i->position.y == toolPosition.y && xOffset > -4 && xOffset < 4) ||
					(i->position.y == toolPosition.y - 1 && xOffset > 0 && xOffset < 8) ||
					(i->position.y == toolPosition.y + 1 && xOffset > -8 && xOffset < 0)) {
					constructionBlocked = true;
					blockReason = "Other " + i->prototype->name + " is in the way";
				}
			}

			const ItemSet &elevators = itemsByType["elevator"];
			for (ItemSet::const_iterator ii = elevators.begin(); !constructionBlocked && ii != elevators.end(); ii++) {
				Item::Item * i = *ii;
				recti itemRect = i->getRect();
				itemRect.origin.y -= 1;
				itemRect.size.y += 2;
				if (toolBoundary.intersectsRect(itemRect)) {
					constructionBlocked = true;
					blockReason = i->prototype->name + " is in the way";
				}
			}

			// Check floor width above
			if (floorItems.count(toolPosition.y + 1) != 0) {
				Item::Item * i = floorItems[toolPosition.y + 1];
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();
			}
			if (toolPosition.x < minFloorX || toolPosition.x + toolPrototype->size.x > maxFloorX) {
				if (!constructionBlocked)
					blockReason = "Upper floor is not wide enough";
				constructionBlocked = true;
			}

			// Check floor width
			minFloorX = INT_MAX;
			maxFloorX = INT_MIN;
			if (floorItems.count(toolPosition.y) != 0) {
				Item::Item * i = floorItems[toolPosition.y];
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();
			}
		} else if (toolPrototype->id.find("elevator") == 0) {
			// Elevator clearances use a two-floor vertical guard band;
			// stairs/escalators themselves are checked against the shaft's
			// uninflated footprint.
			recti elevatorBoundary = toolBoundary;
			elevatorBoundary.origin.x -= Item::Elevator::Elevator::kShaftClearanceCells;
			elevatorBoundary.size.x += 2 * Item::Elevator::Elevator::kShaftClearanceCells;
			elevatorBoundary.origin.y -= 2;
			elevatorBoundary.size.y += 3;

			const ItemSet &stairlike = itemsByType["stairlike"];
			for (ItemSet::const_iterator ii = stairlike.begin(); !constructionBlocked && ii != stairlike.end(); ii++) {
				Item::Item * i = *ii;
				if (toolBoundary.intersectsRect(i->getRect())) {
					constructionBlocked = true;
					blockReason = i->prototype->name + " is in the way";
				}
			}

			const ItemSet &elevators = itemsByType["elevator"];
			for (ItemSet::const_iterator ii = elevators.begin(); !constructionBlocked && ii != elevators.end(); ++ii) {
				Item::Elevator::Elevator *other = (Item::Elevator::Elevator *) *ii;
				recti itemRect = other->getRect();
				// An express segment extending its own column (same x, floor +-1) is an
				// extension, not a neighbouring shaft.
				if (toolPrototype->icon == 6 && other->isExpress() && other->position.x == toolPosition.x &&
				    toolPosition.y + 1 >= other->position.y && toolPosition.y - 1 < other->position.y + other->size.y)
					continue;
				if (elevatorBoundary.intersectsRect(itemRect)) {
					constructionBlocked = true;
					blockReason = "Item requires more space on both sides";
				}
			}


			// Check floor width below/above if constructing above/below ground level
			Item::Item * i = NULL;
			if (toolPosition.y > 0 && floorItems.count(toolPosition.y - 1) != 0)
				i = floorItems[toolPosition.y - 1];
			else if (toolPosition.y < 0 && floorItems.count(toolPosition.y + 1) != 0)
				i = floorItems[toolPosition.y + 1];
			else if (floorItems.count(0) != 0)
				i = floorItems[0];
			if (i) {
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();
			}
		} else {
			if (toolPrototype->icon == ICON_METRO && metroStation != NULL) {
				constructionBlocked = true;
				blockReason = "Only one Metro Station allowed";
			}

			{
				// SECOM Center singleton (OpenSkyWeb ISSUE-031,
				// EXE string "Only one SECOM allowed").
				Game::ItemSetByString::const_iterator sit = itemsByType.find("secom");
				if (toolPrototype->id == "secom" && sit != itemsByType.end() && !sit->second.empty()) {
					constructionBlocked = true;
					blockReason = "Only one SECOM allowed";
				}
			}

			if (toolPosition.y == 0) {
				constructionBlocked = true;
				blockReason = "Only lobbies may be built on the ground floor";
			}

			// Check obstruction from other buildings.
			ItemSet itemsNearby;
			for (int y = 0; !constructionBlocked && y < toolPrototype->size.y; y++) {
				itemsNearby = itemsByFloor[toolPosition.y + y];
				for (ItemSet::const_iterator ii = itemsNearby.begin(); !constructionBlocked && ii != itemsNearby.end(); ii++) {
					Item::Item * i = *ii;
					if (i->canHaulPeople()) continue;
					if (toolPrototype->icon == ICON_FLOOR && i->prototype->id == "skybridge") continue; // split below
					if (toolBoundary.intersectsRect(i->getRect())) {
						constructionBlocked = true;
						blockReason = i->prototype->name + " is in the way";
					}
				}
			}

			// Check floor width below/above if constructing above/below ground level
			Item::Item * i = NULL;
			if (toolPosition.y > 0 && floorItems.count(toolPosition.y - 1) != 0)
				i = floorItems[toolPosition.y - 1];
			else if (floorItems.count(toolPosition.y + toolPrototype->size.y) != 0)
				i = floorItems[toolPosition.y + toolPrototype->size.y];
			if (i) {
				minFloorX = i->position.x;
				maxFloorX = i->getRect().maxX();

				// If the floor below is the top of a multi-story lobby
				// and the new item is wider than the lobby, auto-extend
				// the lobby to match. This mirrors SimTower's behaviour of
				// letting you place condos/offices directly above a sky
				// lobby without manually widening it first. See Phase 2.5.
				if (toolPosition.y > 0 && toolPrototype->icon != ICON_LOBBY) {
					const ItemSet &belowItems = itemsByFloor[toolPosition.y - 1];
					for (ItemSet::const_iterator bi = belowItems.begin(); bi != belowItems.end(); bi++) {
						Item::Item * below = *bi;
						if (below->prototype->icon == ICON_LOBBY && below->size.y > 1) {
							Item::Lobby * l = (Item::Lobby *) below;
							const int lobbyRight = l->position.x + l->size.x;
							const int toolRight  = toolPosition.x + toolPrototype->size.x;
							int diffLeft = 0, diffRight = 0;
							int oldMinX = l->position.x;
							int oldMaxX = l->getRect().maxX();
							const int newMinX = std::min(l->position.x, toolPosition.x);
							const int newMaxX = std::max(lobbyRight, toolRight);
							for (int f = l->position.y; f < l->position.y + l->size.y; ++f) {
								if (!canExtendFloor(*this, f, newMinX, newMaxX)) {
									constructionBlocked = true;
									blockReason = "Horizontal footprint is outside the " +
										std::string(activeRules.displayName()) + " build boundary";
									break;
								}
							}
							if (constructionBlocked) break;

							{
								// Affordability (EXE 1180:00f0): item first, then
								// item + the widening terrain.
								const int wl = std::max(0, l->position.x - toolPosition.x);
								const int wr = std::max(0, toolRight - lobbyRight);
								const int groundBand = l->position.y == 0;
								const int widenCost = groundBand
									? 5000 * l->size.y * (wl + wr)
									: itemFactory.prototypesById["floor"]->price * l->size.y * (wl + wr);
								// The tool's own new deck cells are part of the total.
								long long ownCells = 0;
								for (int i = 0; i < toolPrototype->size.y; i++)
									ownCells += newFloorCells(*this, toolPosition.y + i, toolPosition.x, toolRight);
								const long long ownTerrain = ownCells * itemFactory.prototypesById["floor"]->price;
								if (money.balance < toolPrototype->price) {
									constructionBlocked = true;
									moneyReject = "Not enough money to build this item.";
									break;
								}
								if ((long long)money.balance < (long long)toolPrototype->price + widenCost + ownTerrain) {
									constructionBlocked = true;
									moneyReject = "Not enough money to build the floor.";
									break;
								}
							}

							if (toolPosition.x < l->position.x) {
								diffLeft = l->position.x - toolPosition.x;
								l->size.x += diffLeft;
								l->setPosition(toolPosition);
							}
							if (toolRight > lobbyRight) {
								diffRight = toolRight - lobbyRight;
								l->size.x += diffRight;
							}
							if (diffLeft + diffRight > 0) {
								int newMinX = l->position.x;
								int newMaxX = l->getRect().maxX();
								// Extend floor intervals on every lobby floor.
								for (int f = l->position.y; f < l->position.y + l->size.y; f++) {
									// Ground grand-lobby decks are priced by the lobby
									// rate below, not the $500 floor rate.
									extendFloor(f, newMinX, newMaxX, l->position.y == 0);
									FloorItems::iterator fi = floorItems.find(f);
									if (fi != floorItems.end()) {
										std::multiset<int> &interval = fi->second->interval;
										std::multiset<int>::iterator it = interval.find(oldMinX);
										if (it != interval.end()) interval.erase(it);
										it = interval.find(oldMaxX);
										if (it != interval.end()) interval.erase(it);
										interval.insert(newMinX);
										interval.insert(newMaxX);
									}
								}
								maxFloorX = l->position.x + l->size.x;
								minFloorX = l->position.x;
								// EXE 1180:05fd: ground lobby extension is $5,000 x
								// height x new deck cells; sky lobbies pay only floors.
								if (l->position.y == 0) {
									transferFunds(-5000 * l->size.y * (diffLeft + diffRight), "construction", "Lobby extension");
									playOnce("simtower/construction/flexible");
								}
							}
							break;
						}
					}
				}
			}
		}
		if (toolPosition.x < minFloorX || toolPosition.x + toolPrototype->size.x > maxFloorX) {
			if (!constructionBlocked)
				blockReason = "Floor " + std::string(toolPosition.y > 0 ? "below" : "above") + " is not wide enough";
			constructionBlocked = true;
		}

		if (!constructionBlocked && toolPrototype->icon == ICON_LOBBY) {
			const ItemSet &itemsOnFloor = itemsByFloor[toolPosition.y];
			for (ItemSet::const_iterator ii = itemsOnFloor.begin(); ii != itemsOnFloor.end(); ++ii) {
				Item::Item * item = *ii;
				if (item->prototype->icon != ICON_LOBBY) continue;
				const int newMinX = std::min(item->position.x, toolPosition.x);
				const int newMaxX = std::max(item->getRect().maxX(),
					toolPosition.x + toolPrototype->size.x);
				for (int f = item->position.y; f < item->position.y + item->size.y; ++f) {
					if (!canExtendFloor(*this, f, newMinX, newMaxX)) {
						constructionBlocked = true;
						blockReason = "Horizontal footprint is outside the " +
							std::string(activeRules.displayName()) + " build boundary";
						break;
					}
				}
				break;
			}
		}

		if (!constructionBlocked) {
			LOG(DEBUG, "construct %s at %ix%i, size %ix%i", toolPrototype->id.c_str(), toolPosition.x, toolPosition.y, toolPrototype->size.x, toolPrototype->size.y);

			// Affordability (EXE 1180:00f0): reject on the item price first
			// (message 7), then on item + new-floor terrain (message 8).
			const bool groundLobby = toolPrototype->icon == ICON_LOBBY && toolPosition.y == 0;
			bool lobbyAlreadyThere = false;
			if (toolPrototype->icon == ICON_LOBBY) {
				const ItemSet &lobbyFloor = itemsByFloor[toolPosition.y];
				for (ItemSet::const_iterator ii = lobbyFloor.begin(); ii != lobbyFloor.end(); ++ii)
					if ((*ii)->prototype->icon == ICON_LOBBY) { lobbyAlreadyThere = true; break; }
			}
			int groundLobbyHeight = toolPrototype->size.y;
			if (groundLobby) {
				const ItemSet &gl = itemsByFloor[0];
				for (ItemSet::const_iterator ii = gl.begin(); ii != gl.end(); ++ii)
					if ((*ii)->prototype->icon == ICON_LOBBY) { groundLobbyHeight = (*ii)->size.y; break; }
			}
			long long itemCost = 0, terrainCost = 0;
			if (toolPrototype->icon == ICON_FLOOR || groundLobby || lobbyAlreadyThere)
				itemCost = 0; // floor tool / ground-band lobby / extension of a lobby
			else
				itemCost = toolPrototype->price;
			if (groundLobby) {
				terrainCost = 5000LL * groundLobbyHeight *
					newFloorCells(*this, 0, toolPosition.x, toolPosition.x + toolPrototype->size.x);
			} else {
				long long cells = 0;
				for (int i = 0; i < toolPrototype->size.y; i++)
					cells += newFloorCells(*this, toolPosition.y + i, toolPosition.x,
					                       toolPosition.x + toolPrototype->size.x);
				terrainCost = cells * itemFactory.prototypesById["floor"]->price;
			}
			if ((long long)money.balance < itemCost) {
				moneyReject = "Not enough money to build this item.";
			} else if ((long long)money.balance < itemCost + terrainCost) {
				moneyReject = "Not enough money to build the floor.";
			}
			if (!moneyReject.empty()) {
				constructionBlocked = true;
			} else {

			// Construct floors (ground grand-lobby decks are priced by the
			// lobby rate, charged below)
			const bool yootLobby = activeRules.profile().yootWorld && toolPrototype->icon == ICON_LOBBY;
			for (int i = 0; i < toolPrototype->size.y; i++)
				if (!yootSpecial || activeRules.profile().allowsCell(toolPosition.x, toolPosition.y + i))
					extendFloor(toolPosition.y + i, toolPosition.x, toolPosition.x + toolPrototype->size.x, groundLobby || yootLobby);
			eventSystem.notifyPlaced(); // buried treasure on the build click (Concilia game_notify_built)
			if (groundLobby && terrainCost > 0) {
				transferFunds(-(int)terrainCost, "construction", lobbyAlreadyThere ? "Lobby extension" : toolPrototype->name);
				playOnce("simtower/construction/flexible");
			}
			if (toolPrototype->icon == ICON_LOBBY) {
				// Look for existing lobby to extend
				bool existingLobby = false;
				const ItemSet &itemsOnFloor = itemsByFloor[toolPosition.y];
				for (ItemSet::const_iterator ii = itemsOnFloor.begin(); !existingLobby && ii != itemsOnFloor.end(); ii++) {
					Item::Item * i = *ii;
					if (i->prototype->icon == ICON_LOBBY) {
						gameMap.removeNode(MapNode::Point(i->position.x + i->size.x/2, i->position.y + i->prototype->entrance_offset), i);
						Item::Lobby * l = (Item::Lobby *) i;
						int diff = 0;
						int oldMinX = l->position.x;
						int oldMaxX = l->getRect().maxX();
						if (toolPosition.x < l->position.x) {
							diff = l->position.x - toolPosition.x;
							l->size.x += diff;
							l->setPosition(toolPosition);
						} else {
							diff = toolPosition.x + toolPrototype->size.x - l->getRect().maxX();
							if (diff < 0) diff = 0;
							l->size.x += diff;
						}
						LOG(INFO, "lobby diff = %d", diff);
						l->updateSprite();
						gameMap.addNode(MapNode::Point(i->position.x + i->size.x/2, i->position.y + i->prototype->entrance_offset), i);
						if (diff > 0) {
							int newMinX = l->position.x;
							int newMaxX = l->getRect().maxX();
							for (int f = l->position.y; f < l->position.y + l->size.y; f++) {
								extendFloor(f, newMinX, newMaxX, groundLobby);
								FloorItems::iterator fi = floorItems.find(f);
								if (fi != floorItems.end()) {
									std::multiset<int> &interval = fi->second->interval;
									std::multiset<int>::iterator it = interval.find(oldMinX);
									if (it != interval.end()) interval.erase(it);
									it = interval.find(oldMaxX);
									if (it != interval.end()) interval.erase(it);
									interval.insert(newMinX);
									interval.insert(newMaxX);
								}
							}
							if (yootLobby)
							{
								transferFunds(-Yoot::floorCharge(toolPrototype->price, diff * l->size.y), "construction", "Lobby extension");
								playOnce("simtower/construction/flexible");
							}
							// Extension price is the new-deck terrain charge
							// already booked above (no price*4/diff).
							}
						existingLobby = true;
					}
				}
				// Otherwise construct a new lobby
				if (!existingLobby) {
					Item::Item * item = itemFactory.make(toolPrototype, toolPosition);
					LOG(INFO, "created new lobby item %p", item);
					addItem(item);
					if (yootLobby)
						transferFunds(-Yoot::floorCharge(toolPrototype->price, toolPrototype->size.x * toolPrototype->size.y), "construction", toolPrototype->name);
					// Ground-band lobbies are paid by the deck charge above.
					else if (!groundLobby)
						transferFunds(-toolPrototype->price, "construction", toolPrototype->name);
					playOnce("simtower/construction/normal");
				}
			} else if (toolPrototype->icon != ICON_FLOOR) {
				Item::Item * item = itemFactory.make(toolPrototype, toolPosition);
				addItem(item);
				transferFunds(-toolPrototype->price, "construction", toolPrototype->name);
				if (item->canHaulPeople()) {
					if (item->isElevator()) selectTool("finger");
					updateRoutes();
				} else
					item->updateRoutes();
				playOnce("simtower/construction/normal");
			} else {
				Yoot::skyBridgeFloorBuilt(*this, toolPosition.y, toolPosition.x, toolPosition.x + toolPrototype->size.x);
			}
			} // affordable
		}
		if (constructionBlocked) {
			LOG(DEBUG, "cannot construct %s at %ix%i, size %ix%i, reason: %s", toolPrototype->id.c_str(), toolPosition.x, toolPosition.y, toolPrototype->size.x, toolPrototype->size.y, moneyReject.empty() ? blockReason.c_str() : moneyReject.c_str());
			playOnce("simtower/construction/impossible");
			timeWindow.showMessage(moneyReject.empty() ? "Cannot place item there. " + blockReason + "." : moneyReject);
		}
	}
	if (items.size() != itemsBefore || funds < fundsBefore)
		eventSystem.extras.onBuilt(toolPosition.y + footprintHeight - 1); // T2HotSpring
	return items.size() != itemsBefore;
}

bool Game::isDisasterOfferDay() const
{
	const int day = static_cast<int>(std::floor(time.absolute));
	return day % 60 == 59 || day % 84 == 83;
}

bool Game::isBatchDraggable(Item::AbstractPrototype * proto) const
{
	if (!proto) return false;
	if (proto->id.find("elevator") == 0 || proto->id == "parkingramp") return false;
	if (proto->icon == ICON_LOBBY || proto->icon == ICON_FLOOR
	    || proto->icon == ICON_METRO || proto->icon == ICON_STAIRS
	    || proto->icon == ICON_ELEVATOR) return false;
	return true;
}

void Game::startBatchDrag()
{
	batchDragging = true;
	batchAnchorX = toolPosition.x;
	batchAnchorY = toolPosition.y;
	batchCells.clear();
	batchAffordable = true;
	batchFirstReason.clear();
	updateBatchDrag();
}

void Game::updateBatchDrag()
{
	if (!batchDragging) return;
	if (!isBatchDraggable(toolPrototype)) { cancelBatchDrag(); return; }
	const int dxRaw = toolPosition.x - batchAnchorX;
	const int dyRaw = toolPosition.y - batchAnchorY;
	// A plain drag snaps to the dominant axis (RTS-style line); Shift spans
	// both axes at once (rows x columns grid), preserving drag direction.
	const bool shift = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LShift)
	                || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RShift);
	const bool horizontal = std::abs(dxRaw) >= std::abs(dyRaw);
	const bool useX = shift || horizontal;
	const bool useY = shift || !horizontal;
	const int cols = useX ? std::max(1, (int)std::lround(std::abs((double)dxRaw) / toolPrototype->size.x)) : 1;
	const int rows = useY ? std::max(1, (int)std::lround(std::abs((double)dyRaw) / toolPrototype->size.y)) : 1;
	const int stepX = toolPrototype->size.x * (dxRaw < 0 ? -1 : 1);
	const int stepY = toolPrototype->size.y * (dyRaw < 0 ? -1 : 1);

std::vector<BatchCell> cells;
std::string firstReason;
// Cells are generated anchor-outward, so a vertical drag is ordered
// bottom-to-top (top-to-bottom underground). Simulate the slabs the
// batch creates as we go: a cell with no floor yet is still valid when
// an earlier cell of this same drag will have laid the supporting slab
// by the time it is constructed.
std::set<int> simulatedSlabs;
for (int j = 0; j < rows; j++) {
for (int k = 0; k < cols; k++) {
BatchCell c;
c.x = batchAnchorX + k * stepX;
c.y = batchAnchorY + j * stepY;
c.valid = validateBatchCell(toolPrototype, c.x, c.y, c.reason, simulatedSlabs);
if (!c.valid && firstReason.empty()) firstReason = c.reason;
if (c.valid) {
for (int yy = 0; yy < toolPrototype->size.y; yy++) simulatedSlabs.insert(c.y + yy);
}
cells.push_back(c);
}
}
	batchCells = cells;
	batchAffordable = money.balance >= (long long)cells.size() * toolPrototype->price;
	batchFirstReason = firstReason;
}

void Game::finishBatchDrag()
{
	batchDragging = false;
	if (batchCells.empty()) return;
	const int price = toolPrototype->price;
	int built = 0;
	std::string firstReason = batchFirstReason;
	for (const BatchCell & c : batchCells) {
		if (!c.valid) continue;
		// Stop when the balance can no longer cover another copy.
		if (money.balance < price) break;
		if (constructToolAt(int2(c.x, c.y))) built++;
		else if (firstReason.empty()) firstReason = "placement failed";
	}
	batchCells.clear();
	if (built > 0) {
		updateRoutes();
	} else {
		playOnce("simtower/construction/impossible");
		timeWindow.showMessage("Cannot place item there. " + (firstReason.empty() ? std::string("no space available") : firstReason) + ".");
	}
}

void Game::cancelBatchDrag()
{
	batchDragging = false;
	batchCells.clear();
}

bool Game::validateBatchCell(Item::AbstractPrototype * proto, int x, int y, std::string & reason, const std::set<int> & simulatedSlabs) const
{
    if (eventSystem.isActive()) { reason = "Cannot build during an emergency"; return false; }
    if (eventSystem.rubbleIntersects(recti(int2(x, y), proto->size))) {
        reason = "Bulldoze the burned area before rebuilding"; return false;
    }
	if (!(Yoot::usesSpecialFootprint(*this, proto) ? activeRules.allowsSpecialFootprint(x, y, proto->size.x, proto->size.y)
	                                               : activeRules.allowsFootprint(x, y, proto->size.x, proto->size.y))) {		reason = "Footprint is outside the " +
			std::string(activeRules.displayName()) + " build boundary";
		return false;
	}

	const int minRating = activeRules.minRatingToBuild(proto->id);
	if (minRating == BuildRules::kUnavailable) {
		reason = proto->name + " is not available in " + activeRules.profile().worldName;
		return false;
	}
	if (minRating > rating) {
		reason = proto->name + " unlocks at " + std::to_string(minRating + 1) + " stars";
		return false;
	}
	if (placementRuleBlock(proto, x, y, reason)) return false;
	if (proto->yoot && proto->yoot->maxBuild > 0) {
		Game::ItemSetByString::const_iterator same = itemsByType.find(proto->id);
		if (same != itemsByType.end() && (int)same->second.size() >= proto->yoot->maxBuild) {
			reason = proto->name + " can only be placed once";
			return false;
		}
	}
	reason = Yoot::placementError(*this, proto, x, y, proto->size.y);
	if (!reason.empty()) return false;
	if (activeRules.profile().yootWorld) {
		const int lowestFloor = activeRules.profile().lowestFloor;
		if (y < lowestFloor) { reason = "Cannot build below floor B" + std::to_string(-lowestFloor); return false; }
		if (metroStation != NULL && y < metroStation->position.y) {
			reason = "Cannot build above the metro station";
			return false;
		}
	}	if (proto->id == "secom") {
		Game::ItemSetByString::const_iterator sit = itemsByType.find("secom");
		if (sit != itemsByType.end() && !sit->second.empty()) { reason = "Only one SECOM allowed"; return false; }
	}
	if (y == 0) { reason = "Only lobbies may be built on the ground floor"; return false; }

	const recti boundary(int2(x, y), proto->size);
	for (int yy = 0; yy < proto->size.y; yy++) {
		Game::ItemSetByInt::const_iterator flIt = itemsByFloor.find(y + yy);
		if (flIt == itemsByFloor.end()) continue;
		const ItemSet & itemsOnFloor = flIt->second;
		for (ItemSet::const_iterator ii = itemsOnFloor.begin(); ii != itemsOnFloor.end(); ii++) {
			Item::Item * i = *ii;
			if (i->canHaulPeople()) continue;
			if (boundary.intersectsRect(i->getRect())) {
				reason = i->prototype->name + " is in the way";
				return false;
			}
		}
	}
// Floor support: an existing slab, OR a slab the batch itself will have
// created by the time this cell is constructed (vertical drags build
// bottom-to-top, so each item lays the floor the next one stands on).
const int supportLevel = (y > 0) ? y - 1 : y + proto->size.y;
int minFloorX = INT_MAX;
int maxFloorX = INT_MIN;
bool hasFloor = false;
FloorItems::const_iterator f = (y > 0) ? floorItems.find(y - 1) : floorItems.end();
if (f == floorItems.end()) f = floorItems.find(y + proto->size.y);
if (f != floorItems.end()) {
hasFloor = true;
minFloorX = f->second->position.x;
maxFloorX = f->second->getRect().maxX();
} else if (simulatedSlabs.count(supportLevel)) {
// The batch builds the supporting slab in this same drag.
hasFloor = true;
minFloorX = x;
maxFloorX = x + proto->size.x;
}
if (!hasFloor) {
reason = "Floor " + std::string(y > 0 ? "below" : "above") + " is not wide enough";
return false;
}
if (x < minFloorX || x + proto->size.x > maxFloorX) {
reason = "Floor " + std::string(y > 0 ? "below" : "above") + " is not wide enough";
return false;
}
return true;
}


void Game::selectTool(const char * tool)
{
if (!tool) return;
if (selectedTool != tool) {
cancelBatchDrag(); // a tool change mid-drag invalidates the batch
selectedTool = tool;
		toolboxWindow.updateTool();
		timeWindow.updateTooltip();
	}
}

/** Starts playing the given sound resource once. Releasing the sf::Sound after playback is done
 *  internally. */
void Game::playOnce(Path sound)
{
	//Make sure we don't play sounds to frequently.
	if (soundPlayTimes.count(sound)) {
		if (soundPlayTimes[sound] > time.absolute - 0.25 * Time::kBaseSpeed) {
			return;
		}
	}
	soundPlayTimes[sound] = time.absolute;

	//Actually play the sound.
	Sound * snd = new Sound;
	snd->setBuffer(app.sounds[sound]);
	snd->Play(this);
	autoreleaseSounds.insert(snd);
}

/** Randomly picks one of the items in view and asks it for a background sound that it would like
 * to have played back. */
void Game::playRandomBackgroundSound()
{
    if (eventSystem.isActive()) return;
	sf::RenderWindow &win = app.window;
	const sf::View currentView = win.getView();
	sf::FloatRect view(
		currentView.getCenter() - currentView.getSize() / 2.f,
		currentView.getSize()
	);
	view.position.y = -view.position.y;

	//Pick a random value between 0 and the screen area.
	double r = (double)rand() / RAND_MAX * view.size.x * view.size.y;

	//Iterate through the items that are in view, subtracting each item's area from the random
	//value until it drops below 0. That item will be given the chance to play the sound. This
	//effectively picks a random item in view, weighted by the item areas.
	Item::Item *pick = NULL;
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		if ((*i)->layer != 0) continue;
		const int2 & vp = (*i)->getPositionPixels();
		const sf::Vector2u & vs = (*i)->getSizePixels();
		if (vp.x + vs.x >= view.position.x && vp.x <= (view.position.x + view.size.x) &&
			(vp.y + vs.y) >= (view.position.y - view.size.y) && vp.y <= view.position.y) {
			int area = vs.x * vs.y;
			r -= area;
			if (r <= 0) {
				pick = *i;
				break;
			}
		}
	}

	//Ask the picked item to play the sound.
	if (pick) {
		LOG(DEBUG, "Playing random background sound of %s", pick->desc().c_str());
		Path path = pick->getRandomBackgroundSoundPath();
		if (!path.str().empty()) {
			LOG(DEBUG, "-> sound is %s", path.c_str());
			playOnce(path);
		}
	}
}

/** Called whenever the transportation layout of the tower changes. Items and people should update
 *  their calculated routes here, so they may find new ways of accessing things. */
void Game::updateRoutes()
{
	LOG(DEBUG, "updating routes");
	// Invalidate every cached lobby route and queue the recompute. The
	// searches themselves are drained by pumpRouteBuilds() with a per-frame
	// time budget: eagerly running one A* per room in this call froze large
	// towers for minutes on load (one search per room, ~2800 rooms in a
	// full-lot tower).
	visualizeRoute.clear();
	routeBuildQueue.clear();
	for (ItemSet::iterator i = items.begin(); i != items.end(); i++) {
		(*i)->updateRoutes();
		routeBuildQueue.push_back(*i);
	}
}

void Game::forgetTracking(const Person * p)
{
	judgeSystem.forget(p);
}

void Game::forgetTracking(const Item::Item * item)
{
	judgeSystem.forget(item);
	eventSystem.forgetItem(item);
}

/** Recomputes queued lobby routes, bounded to a few milliseconds per frame
 *  so a full rebuild of a maxed tower spreads over a couple of seconds
 *  instead of blocking the load or the frame loop. */
void Game::pumpRouteBuilds()
{
	if (routeBuildQueue.empty()) return;
	const auto budget = std::chrono::milliseconds(4);
	const auto start = std::chrono::steady_clock::now();
	while (!routeBuildQueue.empty()) {
		Item::Item * item = routeBuildQueue.back();
		routeBuildQueue.pop_back();
		item->computeLobbyRoute();
		// Headless runs (--sim) drain the queue: a wall-clock budget
		// would make route timing depend on host speed.
		if (Sim::deterministic()) continue;
		if (std::chrono::steady_clock::now() - start > budget) break;
	}
}

/** Finds a route from start to destination through the tower. The returned route contains start as
 *  the first and destination as the last node, with transportation in between. If the returned
 *  route is empty(), no path was found through the tower. */
Route Game::findRoute(Item::Item * start, Item::Item * destination, bool serviceRoute)
{
    if (!start || !destination || !items.count(start) || !items.count(destination)) return Route();
	// T2BlackOut::Exec: the end minute triggers escape, not power recovery.
	if (activeRules.profile().yootWorld && eventSystem.power.isBlackOut())
		return Route();	MapNode::Point start_point(start->position.x + start->size.x/2, start->position.y + start->prototype->exit_offset);
	MapNode::Point end_point(destination->position.x + destination->size.x/2, destination->position.y + destination->prototype->entrance_offset);

	// For the special case of Metro, passengers can enter/exit from the middle floor if their destination is on that floor
	if (start->prototype->icon == ICON_METRO && (end_point.y == start_point.y - 1))
		start_point.y = end_point.y;
	else if (destination->prototype->icon == ICON_METRO && (start_point.y == end_point.y - 1))
		end_point.y = start_point.y;

	MapNode *start_mapnode = gameMap.findNode(start_point, start);
	MapNode *destination_mapnode = gameMap.findNode(end_point, destination);
	if (!start_mapnode || !destination_mapnode) return Route();
	return pathFinder.findRoute(start_mapnode, destination_mapnode, start, destination, serviceRoute);
}

void Game::cancelTransientVip()
{
 // The existing reload policy cancels reservations/guest pointers while
 // preserving approvals and review history. Reuse it within owned Game code.
 tinyxml2::XMLPrinter xml;
 xml.OpenElement("vip"); vipSystem.encodeXML(xml); xml.CloseElement();
 tinyxml2::XMLDocument doc;
 if (doc.Parse(xml.CStr()) == tinyxml2::XML_SUCCESS)
  { vipSystem.decodeXML(*doc.RootElement()); vipSystem.relink(); }
}

void Game::destroyItemByDisaster(Item::Item *item)
{
 if (!item || !items.count(item) || item->survivesDisaster()) return;
 const recti footprint = item->getRect();
 const bool transport = item->canHaulPeople();
 const int sale = item->disasterSaleReversal();
 if (sale > 0) {
  // Reverse income, rather than booking a new demolition expense.
  money.balance -= sale; money.todayIncome -= sale; money.quarterIncome -= sale;
  money.todayTotalsByCategory["condo_sale"] -= sale;
  money.quarterTotalsByCategory["condo_sale"] -= sale;
  setFunds(money.balance);
 }
 removeItem(item);
 eventSystem.burnFootprint(footprint);
 if (transport) updateRoutes();
 populationNeedsUpdate = true;
}

void Game::evacuateForDisaster()
{
 cancelBatchDrag();
 draggingElevator = NULL; draggingMotor = 0;
 visualizeRoute.clear();
 inspectorDialog.close(); elevatorDialog.close();
 cancelTransientVip();
 for (Item::Item *item : items) {
  item->underConstruction = false;
  item->constructionEndTime = time.absolute;
 }
 // Preserve owner records for permanent tenants, but park all persons outside
 // the world before venue hooks delete transient guests/customers.
 for (Person *p : people) p->journey.cancel();
 for (Item::Item *item : items) item->evacuateForDisaster();
 for (Item::Item *item : items)
  if (Item::Stairlike *stairs = dynamic_cast<Item::Stairlike *>(item))
   stairs->transitionTimes.clear();
 cockSound.Stop(); morningSound.Stop(); bellsSound.Stop(); eveningSound.Stop();
 for (Sound *sound : autoreleaseSounds) sound->Stop();
 populationNeedsUpdate = true;
}

bool Game::rejectDisasterWorldEdit()
{
 if (!eventSystem.isActive()) return false;
 playOnce("simtower/construction/impossible");
 timeWindow.showMessage("Cannot build or bulldoze during an emergency.");
 return true;
}
