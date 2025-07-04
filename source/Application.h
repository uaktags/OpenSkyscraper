/* Copyright (c) 2012-2015 Fabian Schuiki */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <TGUI/Widgets/MenuBar.hpp>
#include <stack>

#include "BitmapManager.h"
#include "DataManager.h"
#include "FontManager.h"
#include "Logger.h"
#include "Path.h"
#include "State.h"
#include "SoundManager.h"

#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>

#if defined(_MSC_VER)
#include "stdint.h"
#include <direct.h>
#define mkdir(A,B)	_mkdir(A)
#elif defined(_WIN32)
#include <io.h>
#define mkdir(A,B)	mkdir(A)
#else
#include <unistd.h>
#include <errno.h>
#endif

namespace OT
{
	class Application
	{
		friend class State;

	public:
		Application(int argc, char * argv[]);

		Path getPath() const { return path; }

		Logger logger;

		sf::RenderWindow window;
		sf::VideoMode videoMode;
		
		tgui::Gui     gui;
		tgui::MenuBar	menu;
		float uiScale;

		DataManager   data;
		BitmapManager bitmaps;
		FontManager   fonts;
		SoundManager  sounds;

		void pushState(State * state);
		void popState();
		int run();

	private:
		Path path;
		void setPath(const Path & p);

		bool running;
		int exitCode;

		void init();
		void loop();
		void cleanup();

		void makeMenu();

		std::stack<State *> states;

		bool dumpResources;
		Path dumpResourcesPath;
	};

	extern Application * App;
}
