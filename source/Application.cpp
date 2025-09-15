/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <TGUI/Widgets/MenuBar.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#include <vector>
#include <sstream>

#include "Application.h"
#include "Game.h"
#include "MainMenu.h"
#include "SimTowerLoader.h"
#include "OpenGL.h"
#include "tinyxml2.h"

using namespace OT;
using namespace std;

Application * OT::App = NULL;

// Forward declarations
static std::string getUserDataDir();
static void ensureDirExists(const std::string& dir);

// Helper to get user data directory cross-platform
static std::string getUserDataDir() {
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::string dir = std::string(path) + "\\OpenSkyscraper";
        return dir;
    }
    return ".";
#elif defined(__APPLE__)
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/Library/Application Support/OpenSkyscraper";
    }
    return ".";
#else // Linux/Unix
    const char* home = getenv("HOME");
    if (home) {
        return std::string(home) + "/.openskyscraper";
    }
    return ".";
#endif
}

// Helper to ensure directory exists
static void ensureDirExists(const std::string& dir) {
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), NULL);
#else
    mkdir(dir.c_str(), 0755);
#endif
}

Application::Application(int argc, char * argv[])
:	data(this),
	fonts(this),
	bitmaps(this),
	sounds(this)
{
	assert(App == NULL && "Application initialized multiple times");
	App = this;

	assert(argc >= 1 && "argv[0] is required");
	dumpResources = false;
	skipMenu = false;

	// Code to retrieve current working directory
	// May want to move to Boost library for easier path manipulation
	int buf_size = 128;
	char* pwd = NULL;
	char* r_getpwd = NULL;
	do {
		pwd = new char[buf_size];
		r_getpwd = getcwd(pwd, buf_size);
		if(!r_getpwd) {
			delete pwd;
			if(errno == ERANGE)	buf_size *= 2;
			else {
				LOG(ERROR, "Could not retrieve current working directory!");
				exitCode = -1;
				return;
			}
		}
	} while(!r_getpwd);
	std::cout << "pwd = " << pwd << '\n';
	path = Path(pwd);
	delete pwd;
	path = Path(argv[0]).up();
// #ifdef __APPLE__
// 	path = Path("../MacOS").down(path.name());
// #endif

	//Special debug defaults.
#ifdef BUILD_DEBUG
	logger.setLevel(Logger::DEBUG);
	char logname[128];
	snprintf(logname, 128, "debug-%li.log", (long int)time(NULL));
	//logger.setOutputPath(/*dir.down(*/logname/*)*/);
#endif

	//Parse command line arguments.
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "--debug") == 0) {
			logger.setLevel(Logger::DEBUG);
		}
		if (strcmp(argv[i], "--log") == 0) {
			assert(i+1 < argc && "--log is missing path");
			logger.setOutputPath(argv[i++]);
		}
		if (strcmp(argv[i], "--dump-resources") == 0) {
			assert(i+1 < argc && "--dump-resources is missing path");
			dumpResources = true;
			dumpResourcesPath = argv[i+1];
		}
		if (strcmp(argv[i], "--skip-menu") == 0) {
			skipMenu = true;
		}
	}

	LOG(DEBUG,
		"constructed\n"
		"    path     = %s",
		path.str().c_str()
	);
	LOG(IMPORTANT, "ready");
	soundEnabled = false; // TODO: Sound disabled during development - re-enable when sound system is stable

	// UI scale HUD
	uiScaleMessage = "";
	uiScaleMessageTimer = 0.0;
}

/** Runs the application. */
int Application::run()
{
	running = true;
	exitCode = 0;

	if (exitCode == 0) init();
	if (exitCode == 0) loop();
	if (exitCode == 0) cleanup();

	running = false;

	if (exitCode > 0) {
		LOG(ERROR, "exitCode = %i", exitCode);
	}

	return exitCode;
}

void Application::init()
{
	data.init();

	/*WindowsNEExecutable exe;
	DataManager::Paths paths = data.paths("SIMTOWER.EXE");
	bool success;
	for (int i = 0; i < paths.size() && !success; i++) {
		success = exe.load(paths[i]);
	}
	if (!success) {
		LOG(WARNING, "unable to load SimTower executable");
	}
	*/

	SimTowerLoader * simtower = new SimTowerLoader(this);
	if (!simtower->load()) {
		LOG(ERROR, "unable to load SimTower resources");
		exitCode = 1;
		return;
	}
	if (dumpResources) {
		simtower->dump(dumpResourcesPath);
	}
	delete simtower; simtower = NULL;
	//exitCode = 1;

	videoMode.width        = 1280;
	videoMode.height       = 768;
	videoMode.bitsPerPixel = 32;

	window.create(videoMode, "OpenSkyscraper SFML");
	window.setVerticalSyncEnabled(true);

	if (!guiManager.init(&window)) {
		LOG(ERROR, "unable to initialize gui");
		exitCode = 1;
		return;
	}
	

	//Additional GUI stuff.
	// TimeWindowWatch decorator registration removed for TGUI migration
	// TODO: Reimplement watch functionality as TGUI custom widget
	// Rocket::Core::DecoratorInstancer * instancer = new TimeWindowWatchInstancer;
	// Rocket::Core::Factory::RegisterDecoratorInstancer("watch", instancer);
	// instancer->RemoveReference();

	makeMenu(); // Create the top menu bar
	if (skipMenu) {
		pushState(new Game(*this)); // Start directly in game
	} else {
		pushState(new MainMenu(*this)); // Start with the main menu
	}

	// Prefill user data dir with default.tower if missing
	std::string userDir = getUserDataDir();
	ensureDirExists(userDir);
#ifdef _WIN32
	std::string userDefault = userDir + "\\default.tower";
#else
	std::string userDefault = userDir + "/default.tower";
#endif
	struct stat st;
	if (stat(userDefault.c_str(), &st) != 0) {
		// Only copy if it doesn't exist
		DataManager::Paths srcPaths = data.paths("default.tower");
		bool copied = false;
		for (auto& src : srcPaths) {
			FILE* in = fopen(src.c_str(), "rb");
			if (in) {
				FILE* out = fopen(userDefault.c_str(), "wb");
				if (out) {
					char buf[4096];
					size_t n;
					while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
						fwrite(buf, 1, n, out);
					}
					LOG(IMPORTANT, "Copied %s to %s", src.c_str(), userDefault.c_str());
					fclose(out);
					copied = true;
				} else {
					LOG(WARNING, "Could not open %s for writing", userDefault.c_str());
				}
				fclose(in);
				break;
			}
		}
		if (!copied) {
			LOG(WARNING, "Could not prefill default.tower: no source found in data.paths");
		}
	}
}

void Application::loop()
{
	sf::Clock clock;
	sf::Text rateIndicator("<not available>", fonts["UbuntuMono-Regular.ttf"], 16);
	double rateIndicatorTimer = 0;
	double rateDamped = 0;
	double rateDampFactor = 0;
	double dt_max = 0, dt_min = 0;
	int dt_maxmin_resetTimer = 0;

	while (window.isOpen() && exitCode == 0 && !states.empty()) {
		double dt_real = clock.getElapsedTime().asSeconds();
		//dt_max = (dt_max + dt_real * dt_real * 0.5) / (1 + dt_real * 0.5);
		//dt_min = (dt_min + dt_real * dt_real * 0.5) / (1 + dt_real * 0.5);
		if (dt_real > dt_max) {
			dt_max = dt_real;
			dt_maxmin_resetTimer = 0;
		}
		if (dt_real < dt_min) {
			dt_min = dt_real;
			dt_maxmin_resetTimer = 0;
		}
		double dt = std::min<double>(dt_real, 0.1); //avoids FPS dropping below 10 Hz
		clock.restart();

		//Update the rate indicator.
		rateDampFactor = (dt_real * 1);
		rateDamped = (rateDamped + dt_real * rateDampFactor) / (1 + rateDampFactor);
		if ((rateIndicatorTimer += dt_real) > 0.5) {
			rateIndicatorTimer -= 0.5;
			if (++dt_maxmin_resetTimer >= 2*3) {
				dt_maxmin_resetTimer = 0;
				dt_max = dt_real;
				dt_min = dt_real;
			}
		}

		//Handle events.
		sf::Event event;
		while (window.pollEvent(event)) {
			// TGUI event handling
			if (guiManager.getBackend()->handleEvent(event))
				continue;

			// State event handling
			if (!states.empty()) {
				if (states.top()->handleEvent(event))
					continue;
			}

			// Application-level event handling
			if (event.type == sf::Event::Resized) {
				LOG(INFO, "resized (%i, %i)", window.getSize().x, window.getSize().y);
				window.setView(sf::View(sf::FloatRect(0, 0, window.getSize().x, window.getSize().y)));
			}
			if (event.type == sf::Event::KeyPressed) {
				if (event.key.code == sf::Keyboard::Escape) {
					exitCode = 1;
					continue;
				}
				if (event.key.code == sf::Keyboard::R && event.key.control) {
					LOG(IMPORTANT, "reinitializing game");
					Game * old = (Game *)states.top();
					popState();
					delete old;
					Game * game = new Game(*this);
					pushState(game);
					continue;
				}
#if 1
				// Ctrl + / Ctrl -: step through preset UI scales.
				if (event.key.control && (event.key.code == sf::Keyboard::Add || event.key.code == sf::Keyboard::Equal || event.key.code == sf::Keyboard::Subtract || event.key.code == sf::Keyboard::Hyphen)) {
					static const float presets[] = { 1.0f, 1.5f, 2.0f, 2.5f, 3.0f };
					const int presetCount = sizeof(presets)/sizeof(presets[0]);
					float cur = guiManager.getUIScale();
					// Find nearest preset index
					int idx = 0;
					for (int i = 0; i < presetCount; ++i) {
						if (std::fabs(cur - presets[i]) < std::fabs(cur - presets[idx])) idx = i;
					}
					if (event.key.code == sf::Keyboard::Add || event.key.code == sf::Keyboard::Equal) {
						if (idx < presetCount - 1) ++idx;
					} else {
						if (idx > 0) --idx;
					}
					float s = presets[idx];
					guiManager.setUIScale(s);
					// Propagate scale to game world so images/sprites visually scale
					if (!states.empty()) {
						State *st = states.top();
						// Also set the UI scale on the active state's own GUI context
						Game *g = dynamic_cast<Game *>(st);
						if (g) {
							// Because Game::advance multiplies view size by zoom, a smaller
							// zoom value makes world objects appear larger. Use inverse mapping.
							g->setZoom(1.0 / s);
							// Force GUI documents to reload so Rocket reflows with the new scale.
							g->reloadGUI();
						}
					}
					LOG(INFO, "UI scale set to %.2f", s);
					// Resize the main window to match the selected scale relative to
					// the initial video mode, clamped to desktop resolution.
					{
						sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
						unsigned new_w = (unsigned)std::min<unsigned>((unsigned)std::round(videoMode.width * s), desktop.width);
						unsigned new_h = (unsigned)std::min<unsigned>((unsigned)std::round(videoMode.height * s), desktop.height);
						window.setSize(sf::Vector2u(new_w, new_h));
						// Also update the view to the new size so rendering area matches.
						window.setView(sf::View(sf::FloatRect(0, 0, new_w, new_h)));
					}
					// Show transient on-screen message
					char buf[64];
					snprintf(buf, sizeof(buf), "UI scale: %.2fx", s);
					uiScaleMessage = buf;
					uiScaleMessageTimer = 1.5; // show for 1.5 seconds
					continue;
				}
#endif
			}
			if (event.type == sf::Event::Closed) {
				LOG(WARNING, "current state did not handle sf::Event::Closed");
				exitCode = 1;
				continue;
			}
		}

		//Make the current state do its work.
		window.clear(sf::Color(135, 206, 235, 255));  // Sky blue background
		if (!states.empty()) {
			states.top()->advance(dt);
			states.top()->draw(window);
		}
		guiManager.getBackend()->render(window);
		// Draw the debugging overlays.
		char dbg[1024];
		snprintf(dbg, 32, "%.0fHz [%.0f..%.0f]", 1.0/rateDamped, 1.0/dt_max, 1.0/dt_min);
		if (!states.empty()) {
			strcat(dbg, "\n");
			strcat(dbg, states.top()->debugString);
		}
		rateIndicator.setString(dbg);

		window.setView(window.getDefaultView());
		sf::FloatRect r = rateIndicator.getLocalBounds();
		sf::RectangleShape bg = sf::RectangleShape(sf::Vector2f(r.width, r.height));
		bg.setFillColor(sf::Color(0, 0, 0, 0.25*255));
		bg.setPosition(sf::Vector2f(r.left, r.top));
		window.draw(bg);
		window.draw(rateIndicator);

		// Draw transient UI scale message if active.
		if (uiScaleMessageTimer > 0.0) {
			uiScaleMessageTimer -= dt;
			if (uiScaleMessageTimer > 0.0 && !uiScaleMessage.empty()) {
				sf::Text scaleText(uiScaleMessage, fonts["UbuntuMono-Regular.ttf"], 18);
				sf::FloatRect sr = scaleText.getLocalBounds();
				sf::RectangleShape sbg = sf::RectangleShape(sf::Vector2f(sr.width, sr.height));
				sbg.setFillColor(sf::Color(0,0,0, 0.5*255));
				sbg.setPosition(sf::Vector2f(10, 10));
				scaleText.setPosition(sf::Vector2f(10, 10));
				scaleText.setFillColor(sf::Color::White);
				window.draw(sbg);
				window.draw(scaleText);
			} else {
				uiScaleMessageTimer = 0.0;
				uiScaleMessage.clear();
			}
		}

		//Swap buffers.
		window.display();
	}
}

void Application::cleanup()
{
	while (!states.empty()) {
		popState();
	}

	window.close();
}

void Application::saveGameToFile(const std::string& filename) {
    if (states.empty()) return;
    Game* game = dynamic_cast<Game*>(states.top());
    if (!game) return;
    std::string outPath = filename;
    // If filename is not absolute, save in user data dir
    if (filename.find('/') == std::string::npos && filename.find('\\') == std::string::npos) {
        std::string userDir = getUserDataDir();
        ensureDirExists(userDir);
#ifdef _WIN32
        outPath = userDir + "\\" + filename;
#else
        outPath = userDir + "/" + filename;
#endif
    }
    LOG(DEBUG, "Saving to %s", outPath.c_str());
    FILE* f = fopen(outPath.c_str(), "w");
    if (!f) {
        LOG(ERROR, "Could not open %s for writing", outPath.c_str());
        return;
    }
    tinyxml2::XMLPrinter xml(f);
    game->encodeXML(xml);
    fclose(f);
    game->saveFilename = filename;
    game->isDirty = false;
    LOG(IMPORTANT, "Game saved to %s", outPath.c_str());
}

void Application::loadGameFromFile(const std::string& filename) {
    if (states.empty()) return;
    Game* game = dynamic_cast<Game*>(states.top());
    bool createdNewGame = false;
    if (!game) {
        // Not in-game: create a new Game state and pop the menu
        game = new Game(*this);
        popState();
        pushState(game);
        createdNewGame = true;
    }
    tinyxml2::XMLDocument xml;
    std::vector<std::string> tryPaths;
    std::string userDir = getUserDataDir();
#ifdef _WIN32
    tryPaths.push_back(userDir + "\\" + filename);
#else
    tryPaths.push_back(userDir + "/" + filename);
#endif
    tryPaths.push_back(filename); // as given (could be absolute or relative)
    char pwd[512];
    getcwd(pwd, sizeof(pwd));
    LOG(DEBUG, "Current working directory: %s", pwd);
    LOG(DEBUG, "Trying to load: %s", filename.c_str());
    for (const auto& path : tryPaths) {
        LOG(DEBUG, "Trying path: %s", path.c_str());
        if (xml.LoadFile(path.c_str()) == 0) {
            game->decodeXML(xml);
            game->reloadGUI();
            game->saveFilename = filename;
            game->isDirty = false;
            LOG(IMPORTANT, "Game loaded from %s", path.c_str());
            return;
        }
    }
    LOG(ERROR, "Could not load %s from any known location", filename.c_str());
    if (createdNewGame) {
        // If loading failed, return to main menu
        popState();
    }
}

void Application::showFilenameDialog(const std::string& title, const std::string& defaultName, std::function<void(const std::string&)> callback) {
    auto dialog = tgui::ChildWindow::create(title);
    dialog->setSize(300, 120);
    dialog->setPosition((window.getSize().x - dialog->getSize().x) / 2, (window.getSize().y - dialog->getSize().y) / 2);
    dialog->setResizable(false);
    dialog->setTitleButtons(tgui::ChildWindow::TitleButton::None);

    auto editBox = tgui::EditBox::create();
    editBox->setSize(260, 28);
    editBox->setPosition(20, 20);
    editBox->setText(defaultName);
    dialog->add(editBox);

    auto okButton = tgui::Button::create("OK");
    okButton->setSize(80, 28);
    okButton->setPosition(40, 70);
    dialog->add(okButton);

    auto cancelButton = tgui::Button::create("Cancel");
    cancelButton->setSize(80, 28);
    cancelButton->setPosition(180, 70);
    dialog->add(cancelButton);

    okButton->onPress([=]() {
        std::string filename = editBox->getText().toStdString();
        guiManager.getBackend()->getGui()->remove(dialog);
        if (!filename.empty())
            callback(filename);
    });
    cancelButton->onPress([=]() {
        guiManager.getBackend()->getGui()->remove(dialog);
    });
    guiManager.getBackend()->getGui()->add(dialog);
}

void Application::saveGame() {
    if (states.empty()) return;
    Game* game = dynamic_cast<Game*>(states.top());
    if (!game) return;
    if (!game->saveFilename.empty()) {
        saveGameToFile(game->saveFilename);
    } else {
        saveGameAs();
    }
}

void Application::saveGameAs() {
    if (states.empty()) return;
    Game* game = dynamic_cast<Game*>(states.top());
    if (!game) return;
    showFilenameDialog("Save Tower As", "default.tower", [this, game](const std::string& filename) {
        saveGameToFile(filename);
    });
}

void Application::makeMenu() {
    auto menu = tgui::MenuBar::create();
    menu->setHeight(22.f);
    menu->getRenderer()->setTextSize(14);
    menu->addMenu("File");
    menu->addMenuItem("New");
    menu->addMenuItem("Load");
    menu->addMenuItem("Save");
    menu->addMenuItem("Save as...");
    menu->addMenuItem("-");
    menu->addMenuItem("Exit");

    menu->addMenu("Options");
    menu->addMenu("Windows");
    menu->addMenu("Help");
    
    // Fix: Use correct lambda signature for TGUI version
    menu->onMenuItemClick.connect([this](const tgui::String& item){
        if (item == "Exit") {
            window.close();
        } else if (item == "New") {
            // Start a new game (same as MainMenu New Tower)
            Game * game = new Game(*this);
            popState();
            pushState(game);
        } else if (item == "Save") {
            saveGame();
        } else if (item == "Load") {
            loadGame();
        } else if (item == "Save as...") {
            saveGameAs();
        }
    });
    guiManager.getBackend()->getGui()->add(menu);
}

void Application::loadGame() {
    if (states.empty()) return;
    showFilenameDialog("Load Game", "default.tower", [this](const std::string& filename) {
        loadGameFromFile(filename);
    });
}

/** Pushes the given State ontop of the state stack, causing it to receive events. */
void Application::pushState(State * state)
{
	assert(state != NULL && "pushState() requires non-NULL state");
	LOG(INFO, "pushing state %s", typeid(*state).name());
	if (!states.empty()) {
		states.top()->deactivate();
	}
	states.push(state);
	state->activate();
}

/** Pops the topmost State off the state stack, causing the underlying state to regain control.
 *  The application will terminate once there are no more states on the stack. */
void Application::popState()
{
	assert(!states.empty() && "popState() requires at least one state on the states stack");
	LOG(INFO, "popping state %s", typeid(*states.top()).name());
	states.top()->deactivate();
	states.pop();
	if (!states.empty()) {
		states.top()->activate();
	}
}
