/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <TGUI/Widgets/MenuBar.hpp>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

#include "Application.h"
#include "Game.h"
#include "MainMenu.h"
#include "SimTowerLoader.h"
#include "OpenGL.h"

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

void Application::makeMenu() {
	auto menu = tgui::MenuBar::create();
	//menu->setRenderer(std::shared_ptr<RendererData> rendererData)
	menu->setHeight(22.f);
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
