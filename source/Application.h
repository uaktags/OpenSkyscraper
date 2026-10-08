/* Copyright (c) 2012-2015 Fabian Schuiki */
#pragma once

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <TGUI/Widgets/MenuBar.hpp>
#include <stack>
#include <vector>

#include "BitmapManager.h"
#include "DataManager.h"
#include "FontManager.h"
#include "Logger.h"
#include "Path.h"
#include "PluginManager.h"
#include "State.h"
#include "SoundManager.h"

#include "Skin.h"
#include "YootWorld.h"
#include "BuildRules.h"
#include "SimHarness.h"
#include <map>

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
		tgui::MenuBar::Ptr menu = nullptr;
		float uiScale;

		bool audioMuted = false;
		float audioVolume = 100.f;

		bool loadSettings();
		void saveSettings();
		void showSettingsDialog();
		void showNewTowerDialog();
		void showMessageDialog(const std::string & title, const std::string & message);
		bool hasSkinAssets(Skin skin) const;
		const std::string & simTowerExecutablePath() const { return simTowerExecutable; }
		BuildRules::Ruleset selectedRuleset() const { return m_ruleset; }
		bool hasRulesetOverride() const { return m_rulesetExplicit; }
		Skin selectedSkin() const { return m_skin; }
		bool hasSkinOverride() const { return m_skinExplicit; }
		/// YootTower world (--world / New Tower dialog). Default Tokyo.
		YootWorld::Id selectedWorld() const { return m_world; }
		bool hasWorldOverride() const { return m_worldExplicit; }
		void setWorldOverride(YootWorld::Id w) { m_world = w; m_worldExplicit = true; }
		/// Rules for a ruleset; YootTower loads the world plugin through the
		/// plugin catalog (cached per world) and resolves item unlocks.
		BuildRules makeBuildRules(BuildRules::Ruleset ruleset, YootWorld::Id world);
		bool debugMenuEnabled() const { return m_debugMenu; }


		/// Menu-chosen skin (New Tower screen). Same precedence as the
		/// CLI flag: an explicit user selection wins over saved attrs.
		void setSkinOverride(Skin s) { m_skin = s; m_skinExplicit = true; }
		/// Menu-chosen game mode. If the skin wasn't explicitly chosen
		/// (CLI or menu), or if the current skin is not sanctioned for
		/// the new ruleset, it follows the new mode's era default so
		/// e.g. YootTower mode doesn't end up with SimTower Classic art.
		void setRulesetOverride(BuildRules::Ruleset r)
		{
			m_ruleset = r;
			m_rulesetExplicit = true;
			if (!m_skinExplicit || !isSkinSanctioned(r, m_skin))
			{
				Skin def = defaultSkinForRuleset(r);
				m_skin = hasSkinAssets(def) ? def : Skin::OpenSky;
			}
		}
		void setAudioVolume(float volume);
		void setAudioMuted(bool mute);

		DataManager   data;
		BitmapManager bitmaps;
		FontManager   fonts;
		SoundManager  sounds;
		PluginManager pluginManager;

		void pushState(State * state);
		void popState();
		int run();
		void saveGame();
		void loadGame();
		void saveGameToFile(const std::string& filename);
		void loadGameFromFile(const std::string& filename);
		void showFilenameDialog(const std::string& title, const std::string& defaultName, std::function<void(const std::string&)> onOk);
		void saveGameAs();

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
		std::vector<State *> statesToDelete;

		bool dumpResources;
		Path dumpResourcesPath;
		bool startNewGame;
		bool startOfficeLunchQa;
		bool loadGameAtStartup;
		std::string startupLoadPath;
		bool captureFrameAtStartup;
		std::string startupCapturePath;
		bool setMouseAtStartup;
		sf::Vector2i startupMousePosition;
		std::string startupTool;
		bool setViewportScrollAtStartup;
		double startupViewportScrollHorizontal;
		double startupViewportScrollVertical;
		double startupZoomFactor = 0.0; ///< --qa-zoom: Game::zoomBy factor (0 = none)
		bool fullscreen;
		std::string simTowerExecutable;
		/// User-selected folder containing YootTower .t2p plugins
		/// (Settings dialog, "YOOTTOWER RESOURCES"). Empty = automatic
		/// discovery through the data search path.
		std::string yootPluginsPath;
		BuildRules::Ruleset m_ruleset;
		bool m_rulesetExplicit;
		Skin m_skin;
		bool m_skinExplicit;
		YootWorld::Id m_world;
		bool m_worldExplicit;
		std::map<int, RulesProfile> m_yootProfiles;
		bool m_loadedSimTowerResources;
		bool m_debugMenu;
		/// Headless deterministic simulation (--sim); see SimHarness.h.
		Sim::Options m_sim;
		int runSim();
	};

	extern Application * App;
}
