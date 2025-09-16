/* Copyright (c) 2012-2015 Fabian Schuiki */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <stack>

#include "BitmapManager.h"
#include "DataManager.h"
#include "FontManager.h"
#include "GUIManager.h"
#include "Logger.h"
#include "Path.h"
#include "State.h"
#include "SoundManager.h"

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

		DataManager   data;
		GUIManager    guiManager;

		BitmapManager bitmaps;
		FontManager   fonts;
		SoundManager  sounds;

		bool soundEnabled;

		// Transient on-screen UI for showing current UI scale when changed.
		std::string uiScaleMessage;
		double uiScaleMessageTimer; // seconds remaining to display

		int run();
		void quit() { running = false; }

		void saveGame();
		void loadGame();
		void saveGameAs();
		void saveGameToFile(const std::string& filename);
		void loadGameFromFile(const std::string& filename);
		void showFilenameDialog(const std::string& title, const std::string& defaultName, std::function<void(const std::string&)> onOk);

		void pushState(State * state);
		void popState();

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

		// Fine-grained dump flags (set by CLI)
		bool dumpExe;
		bool dumpSprites;
		bool dumpSounds;

		bool skipMenu;
	};

	extern Application * App;
}
