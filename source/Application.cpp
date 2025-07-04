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

Application::Application(int argc, char * argv[])
:	data(this),
	fonts(this),
	bitmaps(this),
	sounds(this)
{
	assert(App == NULL && "Application initialized multiple times");
	App = this;
	uiScale = 1.0f;

	assert(argc >= 1 && "argv[0] is required");
	dumpResources = false;

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
	logger.setOutputPath(/*dir.down(*/logname/*)*/);
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
		if (strcmp(argv[i], "--ui-scale") == 0){
			assert(i+1 < argc && "--ui-scale is missing value");
			uiScale = atof(argv[++i]);
			if (uiScale < 1.0f || uiScale > 2.0f) {
				LOG(ERROR, "invalid ui scale %f", uiScale);
				exitCode = 1;
				return;
			}
			LOG(INFO, "ui scale set to %f", uiScale);
		}
	}

	LOG(DEBUG,
		"constructed\n"
		"    path     = %s",
		path.str().c_str()
	);
	LOG(IMPORTANT, "ready");
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
    dialog->setSize(300 * uiScale, 120 * uiScale);
    dialog->setPosition((window.getSize().x - dialog->getSize().x) / 2, (window.getSize().y - dialog->getSize().y) / 2);
    dialog->setResizable(false);
    dialog->setTitleButtons(tgui::ChildWindow::TitleButton::None);

    auto editBox = tgui::EditBox::create();
    editBox->setSize(260 * uiScale, 28 * uiScale);
    editBox->setPosition(20 * uiScale, 20 * uiScale);
    editBox->setText(defaultName);
    dialog->add(editBox);

    auto okButton = tgui::Button::create("OK");
    okButton->setSize(80 * uiScale, 28 * uiScale);
    okButton->setPosition(40 * uiScale, 70 * uiScale);
    dialog->add(okButton);

    auto cancelButton = tgui::Button::create("Cancel");
    cancelButton->setSize(80 * uiScale, 28 * uiScale);
    cancelButton->setPosition(180 * uiScale, 70 * uiScale);
    dialog->add(cancelButton);

    okButton->onPress([=]() {
        std::string filename = editBox->getText().toStdString();
        gui.remove(dialog);
        if (!filename.empty())
            callback(filename);
    });
    cancelButton->onPress([=]() {
        gui.remove(dialog);
    });
    gui.add(dialog);
}

void Application::saveGame() {
    if (states.empty()) return;
    showFilenameDialog("Save Game As", "default.tower", [this](const std::string& filename) {
        saveGameToFile(filename);
    });
}

void Application::loadGame() {
    if (states.empty()) return;
    showFilenameDialog("Load Game", "default.tower", [this](const std::string& filename) {
        loadGameFromFile(filename);
    });
}

void Application::makeMenu() {
    auto menu = tgui::MenuBar::create();
    menu->setHeight(22.f * uiScale);
    menu->getRenderer()->setTextSize(14 * uiScale); // scale text size as well
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
        }
    });
    gui.add(menu);
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
	//TODO: make this dependent on a command line switch --dump-simtower <path>.
	exe.dump("~/SimTower Raw");*/

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

	gui.setWindow(window);
	makeMenu();
	MainMenu * mainmenu = new MainMenu(*this);
	pushState(mainmenu);

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
    sf::Text rateIndicator("<not available>", fonts["UbuntuMono-Regular.ttf"], 16 * uiScale);
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
// #ifdef BUILD_DEBUG
// 				if (event.key.code == sf::Keyboard::F8) {
// 					bool visible = !Rocket::Debugger::IsVisible();
// 					LOG(DEBUG, "Rocket::Debugger %s", (visible ? "on" : "off"));
// 					Rocket::Debugger::SetVisible(visible);
// 				}
// #endif
            }
            gui.handleEvent(event);
            if (!states.empty()) {
                if (states.top()->handleEvent(event))
                    continue;
                // if (states.top()->gui.handleEvent(event))
                // 	continue;
            }
            if (event.type == sf::Event::Closed) {
                LOG(WARNING, "current state did not handle sf::Event::Closed");
                exitCode = 1;
                continue;
            }
        }

        //Make the current state do its work.
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        window.resetGLStates();
        if (!states.empty()) {
            states.top()->advance(dt);
            window.resetGLStates();
            glEnable(GL_TEXTURE_2D);
            //states.top()->gui.draw();
        }
        gui.draw();

        window.resetGLStates();
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
        // window.draw(bg);
        // window.draw(rateIndicator);


        // cursor crosshair - TODO: replace by sfml cursor
        /*
        sf::Vector2i mp = sf::Mouse::getPosition(window);
        glColor3f(1,0,0);
        glBegin(GL_LINES);
        glVertex2f(mp.x-10,mp.y);
        glVertex2f(mp.x+10,mp.y);
        glVertex2f(mp.x,mp.y-10);
        glVertex2f(mp.x,mp.y+10);
        glEnd();
        glColor3f(1,1,1);
        */

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
