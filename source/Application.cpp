/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <TGUI/Widgets/MenuBar.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include "Skin.h"
#include <TGUI/Widgets/EditBox.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/CheckBox.hpp>
#include <TGUI/Widgets/Slider.hpp>
#include <TGUI/Widgets/ComboBox.hpp>
#include <cmath>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif
#include <vector>
#include <sstream>
#include <memory>

#include "Application.h"
#include "YootWorldArt.h"
#include "Game.h"
#include "MainMenu.h"
#include "NativeFileDialog.h"
#include "SaveHash.h"
#include "SimTowerLoader.h"
#include "TdtLoader.h"
#include "BldImport.h"
#include "BldLoader.h"
#include "OpenGL.h"
#include "tinyxml2.h"

using namespace OT;
using namespace std;

Application *OT::App = NULL;

Application::Application(int argc, char *argv[])
    : data(this),
      fonts(this),
      bitmaps(this),
      sounds(this),
      m_ruleset(BuildRules::Ruleset::OpenSky),
      m_rulesetExplicit(false),
      m_skin(Skin::OpenSky),
      m_skinExplicit(false),
      m_world(YootWorld::Id::Tokyo),
      m_worldExplicit(false),
      m_loadedSimTowerResources(false),
      m_debugMenu(false)
{
    assert(App == NULL && "Application initialized multiple times");
    App = this;
    uiScale = 1.0f;
    audioVolume = 100.f;
    audioMuted = false;
    fullscreen = false;

    bool settingsLoaded = loadSettings();

    assert(argc >= 1 && "argv[0] is required");
    dumpResources = false;
    startNewGame = false;
    startOfficeLunchQa = false;
    loadGameAtStartup = false;
    captureFrameAtStartup = false;
    setMouseAtStartup = false;
    setViewportScrollAtStartup = false;
    m_debugMenu = false;
    startupViewportScrollHorizontal = 0.0;
    startupViewportScrollVertical = 0.0;

    // Code to retrieve current working directory
    // May want to move to Boost library for easier path manipulation
    int buf_size = 128;
    char *pwd = NULL;
    char *r_getpwd = NULL;
    do
    {
        pwd = new char[buf_size];
        r_getpwd = getcwd(pwd, buf_size);
        if (!r_getpwd)
        {
            delete pwd;
            if (errno == ERANGE)
                buf_size *= 2;
            else
            {
                LOG(ERROR, "Could not retrieve current working directory!");
                exitCode = -1;
                return;
            }
        }
    } while (!r_getpwd);
    std::cout << "pwd = " << pwd << '\n';
    path = Path(pwd);
    delete pwd;
    path = Path(argv[0]).up();
    // #ifdef __APPLE__
    // 	path = Path("../MacOS").down(path.name());
    // #endif

    // Special debug defaults.
#ifdef BUILD_DEBUG
    logger.setLevel(Logger::DEBUG);
    char logname[128];
    snprintf(logname, 128, "debug-%li.log", (long int)time(NULL));
    logger.setOutputPath(/*dir.down(*/ logname /*)*/);
#endif

    // Parse command line arguments.
    bool uiScaleExplicit = settingsLoaded;
    for (int i = 1; i < argc; i++)
    {
        if (Sim::parseArg(argc, argv, i, m_sim))
            continue;
        if (strcmp(argv[i], "--debug") == 0)
        {
            logger.setLevel(Logger::DEBUG);
        }
        if (strcmp(argv[i], "--log") == 0)
        {
            assert(i + 1 < argc && "--log is missing path");
            logger.setOutputPath(argv[++i]);
        }
        if (strcmp(argv[i], "--dump-resources") == 0)
        {
            assert(i + 1 < argc && "--dump-resources is missing path");
            dumpResources = true;
            dumpResourcesPath = argv[++i];
        }
        if (strcmp(argv[i], "--simtower-exe") == 0)
        {
            assert(i + 1 < argc && "--simtower-exe is missing path");
            simTowerExecutable = argv[++i];
            LOG(INFO, "SimTower executable override set to %s", simTowerExecutable.c_str());
        }
        if (strcmp(argv[i], "--ruleset") == 0)
        {
            assert(i + 1 < argc && "--ruleset is missing value");
            const char * identifier = argv[++i];
            BuildRules::Ruleset parsed;
            if (BuildRules::parseIdentifier(identifier, parsed))
            {
                m_ruleset = parsed;
                m_rulesetExplicit = true;
                LOG(INFO, "ruleset override set to %s", identifier);
            }
            else
            {
                LOG(ERROR, "invalid ruleset '%s'; expected simtower, opensky, or yoottower", identifier);
            }
        }
        if (strcmp(argv[i], "--world") == 0)
        {
            assert(i + 1 < argc && "--world is missing value");
            const char * identifier = argv[++i];
            YootWorld::Id parsed;
            if (parseYootWorldIdentifier(identifier, parsed))
            {
                setWorldOverride(parsed);
                LOG(INFO, "world override set to %s", identifier);
            }
            else
            {
                LOG(ERROR, "invalid world '%s'; expected tokyo, hawaii, or kegon", identifier);
            }
        }
        if (strcmp(argv[i], "--skin") == 0)
        {
            assert(i + 1 < argc && "--skin is missing value");
            const char * identifier = argv[++i];
            Skin parsed;
            if (parseSkinIdentifier(identifier, parsed))
            {
                m_skin = parsed;
                m_skinExplicit = true;
                LOG(INFO, "skin override set to %s", identifier);
            }
            else
            {
                LOG(ERROR, "invalid skin '%s'; expected classic, opensky, or yoottower", identifier);
            }
        }
        if (strcmp(argv[i], "--debug-menu") == 0)
        {
            m_debugMenu = true;
        }
        if (strcmp(argv[i], "--new-game") == 0)
        {
            startNewGame = true;
        }
        if (strcmp(argv[i], "--load-game") == 0)
        {
            assert(i + 1 < argc && "--load-game is missing path");
            startNewGame = true;
            loadGameAtStartup = true;
            startupLoadPath = argv[++i];
        }
        if (strcmp(argv[i], "--qa-office-lunch") == 0)
        {
            startNewGame = true;
            startOfficeLunchQa = true;
        }
        if (strcmp(argv[i], "--ui-scale") == 0)
        {
            assert(i + 1 < argc && "--ui-scale is missing value");
            uiScaleExplicit = true;
            uiScale = atof(argv[++i]);
            if (uiScale < 1.0f || uiScale > 2.0f)
            {
                LOG(ERROR, "invalid ui scale %f", uiScale);
                exitCode = 1;
                return;
            }
            LOG(INFO, "ui scale set to %f", uiScale);
        }
        if (strcmp(argv[i], "--capture-frame") == 0)
        {
            assert(i + 1 < argc && "--capture-frame is missing path");
            captureFrameAtStartup = true;
            startupCapturePath = argv[++i];
        }
        if (strcmp(argv[i], "--qa-mouse") == 0)
        {
            assert(i + 2 < argc && "--qa-mouse is missing x y");
            setMouseAtStartup = true;
            startupMousePosition.x = atoi(argv[++i]);
            startupMousePosition.y = atoi(argv[++i]);
        }
        if (strcmp(argv[i], "--qa-build") == 0 && i + 3 < argc)
        {
            QaBuild b;
            b.id = argv[++i];
            b.x = atoi(argv[++i]);
            b.floor = atoi(argv[++i]);
            startupBuilds.push_back(b);
        }
        if (strcmp(argv[i], "--qa-tool") == 0)
        {
            assert(i + 1 < argc && "--qa-tool is missing tool id");
            startupTool = argv[++i];
        }
        if (strcmp(argv[i], "--qa-scrollbar") == 0)
        {
            assert(i + 2 < argc && "--qa-scrollbar is missing horizontal vertical");
            setViewportScrollAtStartup = true;
            startupViewportScrollHorizontal = atof(argv[++i]);
            startupViewportScrollVertical = atof(argv[++i]);
        }
        if (strcmp(argv[i], "--qa-zoom") == 0)
        {
            // Viewport zoom factor for captures (2 = the original games' 1:1 scale).
            assert(i + 1 < argc && "--qa-zoom is missing factor");
            startupZoomFactor = atof(argv[++i]);
        }
    }

    // Headless sim: seed every rand() consumer before any Game exists and
    // switch the wall-clock dependent paths to their deterministic form.
    if (m_sim.enabled)
    {
        startNewGame = true;
        Sim::setDeterministic(true);
        srand(m_sim.seed);
    }

    // The skin defaults to the ruleset's own era unless --skin was given.
    if (!m_skinExplicit)
        m_skin = defaultSkinForRuleset(m_ruleset);
    else if (!isSkinSanctioned(m_ruleset, m_skin))
    {
        LOG(WARNING, "Skin '%s' is not supported in ruleset '%s'; switching to %s",
            skinDisplayName(m_skin), BuildRules(m_ruleset).displayName(),
            skinDisplayName(defaultSkinForRuleset(m_ruleset)));
        m_skin = defaultSkinForRuleset(m_ruleset);
    }

    if (!uiScaleExplicit)
    {
        sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
        float scaleX = desktop.size.x / 1280.0f;
        float scaleY = desktop.size.y / 768.0f;
        uiScale = std::max(1.0f, std::min(2.0f, std::min(scaleX, scaleY)));
        LOG(INFO, "auto ui scale = %f (desktop %ux%u)", uiScale, desktop.size.x, desktop.size.y);
    }

    LOG(DEBUG,
        "constructed\n"
        "    path     = %s",
        path.str().c_str());
    LOG(IMPORTANT, "ready");
}

/** Runs the application. */
int Application::run()
{
    running = true;
    exitCode = 0;

    if (exitCode == 0)
        init();
    if (exitCode == 0 && m_sim.enabled)
        return runSim();
    if (exitCode == 0)
        loop();
    if (exitCode == 0)
        cleanup();

    running = false;

    if (exitCode > 0)
    {
        LOG(ERROR, "exitCode = %i", exitCode);
    }

    return exitCode;
}

// Helper to get user data directory cross-platform
static std::string getUserDataDir()
{
#ifdef _WIN32
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        std::string dir = std::string(path) + "\\OpenSkyscraper";
        return dir;
    }
    return ".";
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (home)
    {
        return std::string(home) + "/Library/Application Support/OpenSkyscraper";
    }
    return ".";
#else // Linux/Unix
    const char *home = getenv("HOME");
    if (home)
    {
        return std::string(home) + "/.openskyscraper";
    }
    return ".";
#endif
}

// Helper to ensure directory exists
static void ensureDirExists(const std::string &dir)
{
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), NULL);
#else
    mkdir(dir.c_str(), 0755);
#endif
}

static bool isAbsoluteOrQualifiedPath(const std::string &filename)
{
    if (filename.empty())
        return false;
    if (filename[0] == '/' || filename[0] == '\\')
        return true;
    return filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos;
}

static std::string userDataPath(const std::string &filename)
{
    if (isAbsoluteOrQualifiedPath(filename))
        return filename;

    std::string userDir = getUserDataDir();
    ensureDirExists(userDir);
#ifdef _WIN32
    return userDir + "\\" + filename;
#else
    return userDir + "/" + filename;
#endif
}

void Application::saveGameToFile(const std::string &filename)
{
    if (states.empty())
        return;
    Game *game = dynamic_cast<Game *>(states.top());
    if (!game)
        return;
    std::string outPath = filename;
    // If filename is not absolute, save in user data dir
    if (!isAbsoluteOrQualifiedPath(filename))
        outPath = userDataPath(filename);
    LOG(DEBUG, "Saving to %s", outPath.c_str());
    if (!game->writeTowerFile(outPath))
    {
        return;
    }
    game->saveFilename = filename;
    game->isDirty = false;
    LOG(IMPORTANT, "Game saved to %s", outPath.c_str());
}

void Application::loadGameFromFile(const std::string &filename)
{
    if (states.empty())
        return;
    Game *game = dynamic_cast<Game *>(states.top());
    bool createdNewGame = false;
    if (!game)
    {
        // Not in-game: prepare a new Game state, but keep the current state
        // alive until the save has been loaded successfully.
        game = new Game(*this);
        createdNewGame = true;
    }
    // Original 1994 SimTower .TDT saves: sniff the little-endian 0x2400
    // version word and convert to our native XML tower format before the
    // regular decode path. Import never writes back to the .tdt file.
    if (OT::Tdt::looksLikeTdt(filename))
    {
        OT::Tdt::TdtData tdt;
        std::string tdtError;
        if (!OT::Tdt::loadFile(filename, tdt, tdtError))
        {
            LOG(ERROR, "Could not import SimTower save %s: %s", filename.c_str(), tdtError.c_str());
            if (createdNewGame) delete game;
            return;
        }
        std::vector<std::string> report;
        const std::string xmlText = OT::Tdt::toTowerXml(tdt, report);
        tinyxml2::XMLDocument xml;
        if (xml.Parse(xmlText.c_str(), xmlText.size()) != tinyxml2::XML_SUCCESS)
        {
            LOG(ERROR, "TDT import produced invalid XML for %s", filename.c_str());
            if (createdNewGame) delete game;
            return;
        }
        if (!hasRulesetOverride())
        {
            game->setRules(BuildRules(BuildRules::Ruleset::SimTower));
            LOG(INFO, "TDT import selected SimTower ruleset");
        }
        game->decodeXML(xml);
        game->reloadGUI();
        // Point autosave/save at our own extension so the original save is
        // never overwritten with our format.
        std::string nativeName = filename;
        if (nativeName.size() >= 4 && nativeName.compare(nativeName.size() - 4, 4, ".tdt") == 0)
            nativeName = nativeName.substr(0, nativeName.size() - 4) + ".tower";
        game->saveFilename = nativeName;
        game->isDirty = false;
        for (size_t i = 0; i < tdt.warnings.size(); i++)
            LOG(WARNING, "TDT import: %s", tdt.warnings[i].c_str());
        for (size_t i = 0; i < report.size(); i++)
            LOG(INFO, "TDT import: %s", report[i].c_str());
        LOG(IMPORTANT, "SimTower save imported from %s", filename.c_str());
        if (createdNewGame)
        {
            popState();
            pushState(game);
        }
        return;
    }
    // YootTower .bld saves ('002T' magic): converted to native XML in
    // YootTower mode by BldLoader; the .bld is never written back.
    if (OT::Bld::looksLikeBld(filename))
    {
        if (!OT::importBldSave(*game, filename))
        {
            if (createdNewGame) delete game;
            return;
        }
        if (createdNewGame)
        {
            popState();
            pushState(game);
        }
        return;
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
    for (const auto &path : tryPaths)
    {
        LOG(DEBUG, "Trying path: %s", path.c_str());
        // Read the raw text first so the integrity hash (OpenSkyWeb
        // savehash.js port) can be verified over exactly what we parse.
        std::ifstream in(path.c_str(), std::ios::binary);
        if (!in) continue;
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string text = buffer.str();
        const SaveHash::VerifyResult integrity = SaveHash::verifySaveHash(text);
        if (!integrity.valid)
        {
            LOG(ERROR, "Save integrity check failed for %s (stored %s, computed %s) - refusing to load; the file may be corrupted or modified",
                path.c_str(), integrity.stored.c_str(), integrity.computed.c_str());
            continue;
        }
        if (xml.Parse(text.c_str()) == tinyxml2::XML_SUCCESS)
        {
            // A native save's attribute (or the safe OpenSky fallback for
            // an old save) owns its ruleset. Clear any prior TDT import
            // preselection before Game decodes this normal XML load.
            if (!hasRulesetOverride())
                game->setRules(makeBuildRules(selectedRuleset(), selectedWorld()));
            game->decodeXML(xml);
            game->reloadGUI();
            game->saveFilename = filename;
            game->isDirty = false;
            if (createdNewGame)
            {
                popState();
                pushState(game);
            }
            LOG(IMPORTANT, "Game loaded from %s", path.c_str());
            return;
        }
    }
    LOG(ERROR, "Could not load %s from any known location", filename.c_str());
    if (createdNewGame)
    {
        delete game;
    }
}

void Application::showFilenameDialog(const std::string &title, const std::string &defaultName, std::function<void(const std::string &)> callback)
{
    auto dialog = tgui::ChildWindow::create(title);
    dialog->setClientSize({300 * uiScale, 110 * uiScale});
    dialog->getRenderer()->setTitleBarHeight(24 * uiScale);
    dialog->setTitleTextSize(14 * uiScale);
    dialog->getRenderer()->setTitleColor(sf::Color(220, 220, 220));
    dialog->getRenderer()->setBackgroundColor(sf::Color(30, 30, 35));
    dialog->getRenderer()->setTitleBarColor(sf::Color(45, 45, 50));
    dialog->getRenderer()->setBorderColor(sf::Color(80, 80, 90));
    dialog->getRenderer()->setBorders(2);
    dialog->setPosition({(window.getSize().x - dialog->getSize().x) / 2.f,
                         (window.getSize().y - dialog->getSize().y) / 2.f});
    dialog->setResizable(false);
    dialog->setTitleButtons(tgui::ChildWindow::TitleButton::None);

    auto editBox = tgui::EditBox::create();
    editBox->setSize(260 * uiScale, 28 * uiScale);
    editBox->setTextSize(14 * uiScale);
    editBox->setPosition((dialog->getClientSize().x - editBox->getSize().x) / 2.f,
                         18 * uiScale);
    editBox->setText(defaultName);
    editBox->getRenderer()->setBackgroundColor(sf::Color(45, 45, 50));
    editBox->getRenderer()->setBackgroundColorHover(sf::Color(50, 50, 56));
    editBox->getRenderer()->setTextColor(sf::Color(240, 240, 240));
    editBox->getRenderer()->setDefaultTextColor(sf::Color(160, 160, 165));
    editBox->getRenderer()->setSelectedTextColor(sf::Color::White);
    editBox->getRenderer()->setSelectedTextBackgroundColor(sf::Color(0, 122, 204));
    editBox->getRenderer()->setCaretColor(sf::Color::White);
    editBox->getRenderer()->setCaretColorHover(sf::Color::White);
    editBox->getRenderer()->setBorderColor(sf::Color(80, 80, 90));
    editBox->getRenderer()->setBorders(1);
    dialog->add(editBox);

    auto okButton = tgui::Button::create("OK");
    okButton->setSize(80 * uiScale, 28 * uiScale);
    okButton->setTextSize(14 * uiScale);
    okButton->setPosition(45 * uiScale,
                          dialog->getClientSize().y - 45 * uiScale);
    okButton->getRenderer()->setBackgroundColor(sf::Color(0, 122, 204));
    okButton->getRenderer()->setTextColor(sf::Color::White);
    okButton->getRenderer()->setBackgroundColorHover(sf::Color(20, 142, 224));
    okButton->getRenderer()->setBorderColor(sf::Color(0, 102, 184));
    okButton->getRenderer()->setBorders(1);
    okButton->getRenderer()->setRoundedBorderRadius(4 * uiScale);
    dialog->add(okButton);

    auto cancelButton = tgui::Button::create("Cancel");
    cancelButton->setSize(80 * uiScale, 28 * uiScale);
    cancelButton->setTextSize(14 * uiScale);
    cancelButton->setPosition(dialog->getClientSize().x - 125 * uiScale,
                              dialog->getClientSize().y - 45 * uiScale);
    cancelButton->getRenderer()->setBackgroundColor(sf::Color(60, 60, 65));
    cancelButton->getRenderer()->setTextColor(sf::Color::White);
    cancelButton->getRenderer()->setBackgroundColorHover(sf::Color(80, 80, 85));
    cancelButton->getRenderer()->setBorderColor(sf::Color(50, 50, 55));
    cancelButton->getRenderer()->setBorders(1);
    cancelButton->getRenderer()->setRoundedBorderRadius(4 * uiScale);
    dialog->add(cancelButton);

    okButton->onPress([this, editBox, dialog, callback]()
                      {
        std::string filename = editBox->getText().toStdString();
        gui.remove(dialog);
        if (!filename.empty())
            callback(filename); });
    cancelButton->onPress([this, dialog]()
                          { gui.remove(dialog); });
    gui.add(dialog);
}

void Application::saveGame()
{
    if (states.empty())
        return;
    Game *game = dynamic_cast<Game *>(states.top());
    if (!game)
        return;
    if (!game->saveFilename.empty())
    {
        saveGameToFile(game->saveFilename);
    }
    else
    {
        saveGameAs();
    }
}

void Application::saveGameAs()
{
    if (states.empty())
        return;
    Game *game = dynamic_cast<Game *>(states.top());
    if (!game)
        return;
    std::string filename;
    const std::string defaultName = game->saveFilename.empty() ? "default.tower" : game->saveFilename;
    const std::string defaultPath = userDataPath(defaultName);
    NativeFileDialog::Result result = NativeFileDialog::saveTowerFile("Save Tower As", defaultPath, filename);
    if (result == NativeFileDialog::Result::Selected)
    {
        saveGameToFile(filename);
        return;
    }
    if (result == NativeFileDialog::Result::Cancelled)
        return;

    showFilenameDialog("Save Tower As", defaultName, [this, game](const std::string &filename)
                       { saveGameToFile(filename); });
}

void Application::makeMenu()
{
    menu = tgui::MenuBar::create();
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
    menu->addMenuItem("Settings...");
    menu->addMenuItem("Fullscreen");

    menu->addMenu("Windows");
    menu->addMenuItem("Toolbox");
    menu->addMenuItem("Time");
    menu->addMenuItem("-");
    menu->addMenuItem("Reset Layout");

    menu->addMenu("Help");

    // Fix: Use correct lambda signature for TGUI version
    menu->onMenuItemClick([this](const std::vector<tgui::String> &items)
                          {
            if (items.back() == "Exit") {
                window.close();
            }
            else if (items.back() == "New") {
                showNewTowerDialog();
            }
            else if (items.back() == "Save") {
                saveGame();
            }
            else if (items.back() == "Load") {
                loadGame();
            }
            else if (items.back() == "Save as...") {
                saveGameAs();
            }
            else if (items.back() == "Settings...") {
                showSettingsDialog();
            }
            else if (items.back() == "Fullscreen") {
                fullscreen = !fullscreen;
                if (fullscreen) {
                    videoMode = sf::VideoMode::getDesktopMode();
                    window.create(videoMode, "OpenSkyscraper SFML", sf::Style::Default, sf::State::Fullscreen);
                } else {
                    videoMode = sf::VideoMode({static_cast<unsigned>(1280 * uiScale), static_cast<unsigned>(768 * uiScale)});
                    window.create(videoMode, "OpenSkyscraper SFML", sf::Style::Default, sf::State::Windowed);
                }
                window.setVerticalSyncEnabled(true);
                gui.setWindow(window);
                saveSettings();
            }
            else if (items.back() == "Toolbox" || items.back() == "Time" || items.back() == "Reset Layout") {
                if (states.empty()) return;
                Game* game = dynamic_cast<Game*>(states.top());
                if (!game) return;
                if (items.back() == "Toolbox") {
                    game->toolboxWindow.window->setVisible(!game->toolboxWindow.window->isVisible());
                }
                else if (items.back() == "Time") {
                    game->timeWindow.setVisible(!game->timeWindow.isVisible());
                }
                else if (items.back() == "Reset Layout") {
                    game->toolboxWindow.window->setVisible(true);
                    game->timeWindow.setVisible(true);
                    game->reloadGUI();
                }
            } });
    gui.add(menu);
}

// Builds the PluginManager catalog by scanning every configured data dir
// for a Plugins/ subdirectory. Idempotent across re-scans (PluginManager
// de-duplicates). After this returns, app.pluginManager reflects every
// .t2p discoverable through the runtime search path.
static void catalogPlugins(Application * app)
{
    for (const Path & base : app->data.searchDirs())
    {
        Path plugins = base + "Plugins";
        app->pluginManager.addPluginDirectory(plugins);
    }
}

void Application::init()
{
    data.init();

    // A user-selected executable takes precedence over all automatically
    // discovered resource locations. SimTowerLoader still handles whether
    // the selected directory contains SIMTOWER.EXE or compressed EX_ media.
    if (!simTowerExecutable.empty())
        data.addSearchPath(Path(simTowerExecutable).up());

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

    // --dump-resources is a CLI-only mode that extracts assets from
    // SIMTOWER.EXE and exits. No window, no fallbacks, no dialogs.
    if (dumpResources)
    {
        SimTowerLoader simtower(this);
        if (simtower.load())
            simtower.dump(dumpResourcesPath);
        else
            LOG(WARNING, "resource dumping requires SIMTOWER.EXE or SIMTOWER.EX_");
        return;
    }


    // Acquire optional legacy game resources in order of preference:
    //   1. Extract from SIMTOWER.EXE / SIMTOWER.EX_
    //   2. Pre-dumped bitmaps/sounds under data/bitmaps and data/sounds
    //   3. Any independently discovered plugins under data/Plugins
    // None of these are startup requirements: first-party resources and
    // safe placeholders allow a clean standalone installation to launch.
    m_loadedSimTowerResources = false;
    // The skin must be active BEFORE the loader runs: the EXE-sheet
    // install gate reads BitmapManager's skin to decide which extracted
    // sheets get installed.
    bitmaps.setSkin(selectedSkin());
    {
        SimTowerLoader simtower(this);
        m_loadedSimTowerResources = simtower.load();
    }

    // Detect pre-dumped resources by real file presence. Note that
    // BitmapManager/SoundManager always "succeed" by synthesising placeholder
    // assets, so we can't use their return value to gate startup.
    bool usingDumpedResources = false;
    if (!m_loadedSimTowerResources)
    {
        usingDumpedResources =
            data.exists("bitmaps/sky.png") &&
            data.exists("bitmaps/ui/toolbox/tools.png") &&
            data.exists("sounds/cock.wav");
    }

    // The Classic skin is canonical EXE art and needs SIMTOWER.EXE. Without
    // it there is nothing classic to show: fall back to OpenSky art rather
    // than launching with placeholders everywhere.
    if (selectedSkin() == Skin::Classic && !m_loadedSimTowerResources)
    {
        LOG(WARNING, "Classic skin requires SIMTOWER.EXE; falling back to OpenSky art");
        m_skin = Skin::OpenSky;
        bitmaps.setSkin(m_skin);
    }
    // A user-selected YootTower plugin folder (Settings dialog) is
    // cataloged alongside the automatic search-path Plugins/ dirs, so
    // SIMTOWER.EXE and YootTower media can both be provided at once.
    if (!yootPluginsPath.empty())
        pluginManager.addPluginDirectory(Path(yootPluginsPath));
    // Build the plugin catalog from every search dir's Plugins/ subfolder.
    // Plugins are optional and lazily loaded when a resource or item asks for
    // one. This includes both YootTower .t2p files and OpenTower directories.
    catalogPlugins(this);
    const std::vector<std::string> discoveredPlugins = pluginManager.enumerate();
    std::size_t openTowerPluginCount = 0;
    for (const std::string &name : discoveredPlugins)
        if (name.compare(0, 10, "OpenTower/") == 0)
            ++openTowerPluginCount;
    const std::size_t externalPluginCount = discoveredPlugins.size() - openTowerPluginCount;
    LOG(INFO, "cataloged %zu first-party and %zu optional external plugin(s)",
        openTowerPluginCount, externalPluginCount);

    // The YootTower skin draws from the world plugin and Floor.t2p, so it
    // can only be judged once the catalog exists.
    if (selectedSkin() == Skin::Yoot && !hasSkinAssets(Skin::Yoot))
    {
        LOG(WARNING, "YootTower skin needs %s and %s in a Plugins folder; falling back to OpenSky art",
            yootWorldPluginFile(selectedWorld()), YootWorldArt::floorPluginFor(selectedWorld()));
        m_skin = Skin::OpenSky;
    }
    // Activate the selected skin once every art provider is cataloged, so
    // BitmapManager knows which provider wins for each texture key.
    bitmaps.setSkin(selectedSkin());

    // Report which resource path we ended up on.
    if (m_loadedSimTowerResources)
    {
        LOG(INFO, "loaded resources from SIMTOWER.EXE");
    }
    else if (usingDumpedResources)
    {
        LOG(WARNING, "using dumped SimTower resources from the data directory");
    }
    else
    {
        LOG(INFO, "running with standalone resources; missing optional assets use safe fallbacks");
    }
    if (externalPluginCount != 0)
        LOG(INFO, "%zu external plugin(s) available as optional content", externalPluginCount);

    videoMode = sf::VideoMode({static_cast<unsigned>(1280 * uiScale), static_cast<unsigned>(768 * uiScale)});

    if (fullscreen)
    {
        videoMode = sf::VideoMode::getDesktopMode();
        window.create(videoMode, "OpenSkyscraper SFML", sf::Style::Default, sf::State::Fullscreen);
    }
    else
    {
        window.create(videoMode, "OpenSkyscraper SFML");
    }
    if (m_sim.enabled)
        window.setVisible(false);
    window.setVerticalSyncEnabled(true);

    gui.setWindow(window);
    makeMenu();
    if (startNewGame)
    {
        Game *game = new Game(*this);
        game->saveFilename = "";
        game->isDirty = false;
        if (startOfficeLunchQa)
            game->seedOfficeLunchQa();
        else if (!loadGameAtStartup)
            game->seedNewTower();
        pushState(game);
        if (loadGameAtStartup)
            loadGameFromFile(startupLoadPath);
        if (!startupTool.empty())
            game->selectTool(startupTool.c_str());
        for (const QaBuild & b : startupBuilds)
            game->qaBuild(b.id, int2(b.x, b.floor));
        if (setViewportScrollAtStartup)
            game->qaSetViewportScrollFractions(startupViewportScrollHorizontal, startupViewportScrollVertical);
        if (startupZoomFactor > 0.0)
            game->zoomBy(startupZoomFactor);
        if (setMouseAtStartup)
            sf::Mouse::setPosition(startupMousePosition, window);
    }
    else
    {
        MainMenu *mainmenu = new MainMenu(*this);
        pushState(mainmenu);
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
    if (stat(userDefault.c_str(), &st) != 0)
    {
        // Only copy if it doesn't exist
        DataManager::Paths srcPaths = data.paths("default.tower");
        bool copied = false;
        for (auto &src : srcPaths)
        {
            FILE *in = fopen(src.c_str(), "rb");
            if (in)
            {
                FILE *out = fopen(userDefault.c_str(), "wb");
                if (out)
                {
                    char buf[4096];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
                    {
                        fwrite(buf, 1, n, out);
                    }
                    LOG(IMPORTANT, "Copied %s to %s", src.c_str(), userDefault.c_str());
                    fclose(out);
                    copied = true;
                }
                else
                {
                    LOG(WARNING, "Could not open %s for writing", userDefault.c_str());
                }
                fclose(in);
                break;
            }
        }
        if (!copied)
        {
            LOG(WARNING, "Could not prefill default.tower: no source found in data.paths");
        }
    }
}

void Application::loop()
{
    sf::Clock clock;
    sf::Text rateIndicator(fonts["UbuntuMono-Regular.ttf"], "<not available>", 16 * uiScale);
    double rateIndicatorTimer = 0;
    double rateDamped = 0;
    double rateDampFactor = 0;
    double dt_max = 0, dt_min = 0;
    int dt_maxmin_resetTimer = 0;

    while (window.isOpen() && exitCode == 0 && !states.empty())
    {
        for (State *state : statesToDelete)
        {
            delete state;
        }
        statesToDelete.clear();

        double dt_real = clock.restart().asSeconds();
        if (dt_real > dt_max)
        {
            dt_max = dt_real;
            dt_maxmin_resetTimer = 0;
        }
        if (dt_real < dt_min)
        {
            dt_min = dt_real;
            dt_maxmin_resetTimer = 0;
        }
        double dt = std::min<double>(dt_real, 0.1); // avoids FPS dropping below 10 Hz

        // Update the rate indicator.
        rateDampFactor = (dt_real * 1);
        rateDamped = (rateDamped + dt_real * rateDampFactor) / (1 + rateDampFactor);
        if ((rateIndicatorTimer += dt_real) > 0.5)
        {
            rateIndicatorTimer -= 0.5;
            if (++dt_maxmin_resetTimer >= 2 * 3)
            {
                dt_maxmin_resetTimer = 0;
                dt_max = dt_real;
                dt_min = dt_real;
            }
        }

        // Handle events.
        while (const std::optional event = window.pollEvent())
        {
            if (const auto *resized = event->getIf<sf::Event::Resized>())
            {
                LOG(INFO, "resized (%i, %i)", resized->size.x, resized->size.y);
                sf::View view(sf::FloatRect({0, 0}, {static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)}));
                window.setView(view);
                gui.setAbsoluteView({0.f, 0.f, static_cast<float>(resized->size.x), static_cast<float>(resized->size.y)});
            }
            else if (const auto *keyPressed = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyPressed->code == sf::Keyboard::Key::Escape)
                {
                    exitCode = 1;
                    continue;
                }
                if (keyPressed->code == sf::Keyboard::Key::R && (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::LControl) || sf::Keyboard::isKeyPressed(sf::Keyboard::Key::RControl)))
                {
                    LOG(IMPORTANT, "reinitializing game");
                    popState();
                    Game *game = new Game(*this);
                    pushState(game);
                    continue;
                }
            }
            else if (event->is<sf::Event::Closed>())
            {
                LOG(WARNING, "current state did not handle sf::Event::Closed");
                exitCode = 1;
                continue;
            }
            gui.handleEvent(*event);
            if (!states.empty())
            {
                auto currentEvent = *event;
                if (states.top()->handleEvent(currentEvent))
                    continue;
            }
        }

        // Make the current state do its work.
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        window.resetGLStates();
        if (!states.empty())
        {
            states.top()->advance(dt);
            window.resetGLStates();
            glEnable(GL_TEXTURE_2D);
        }
        gui.draw();
        if (!states.empty())
        {
            Game *game = dynamic_cast<Game *>(states.top());
            if (game)
                game->drawViewportScrollbars(window);
        }

        window.resetGLStates();
        // Draw the debugging overlays.
        char dbg[1024];
        snprintf(dbg, 32, "%.0fHz [%.0f..%.0f]", 1.0 / rateDamped, 1.0 / dt_max, 1.0 / dt_min);
        if (!states.empty())
        {
            strcat(dbg, "\n");
            strcat(dbg, states.top()->debugString);
        }
        rateIndicator.setString(dbg);

        window.setView(window.getDefaultView());
        sf::FloatRect r = rateIndicator.getLocalBounds();
        sf::RectangleShape bg(sf::Vector2f(r.size.x, r.size.y));
        bg.setFillColor(sf::Color(0, 0, 0, 64)); // 0.25*255
        bg.setPosition(r.position);
        // window.draw(bg);
        // window.draw(rateIndicator);

        if (captureFrameAtStartup)
        {
            sf::Texture frame(window.getSize());
            frame.update(window);
            if (!frame.copyToImage().saveToFile(startupCapturePath))
                LOG(ERROR, "unable to save frame capture to %s", startupCapturePath.c_str());
            std::exit(0);
        }

        // Swap buffers.
        window.display();
    }
}

void Application::cleanup()
{
    saveSettings();
    while (!states.empty())
    {
        popState();
    }
    for (State *state : statesToDelete)
    {
        delete state;
    }
    statesToDelete.clear();

    window.close();
}

/** Runs the headless --sim session on the startup game, then tears the
 *  states down without the interactive loop or a settings write. */
int Application::runSim()
{
    Game *game = states.empty() ? NULL : dynamic_cast<Game *>(states.top());
    int code = 2;
    if (!game)
        LOG(ERROR, "sim: no game state to simulate");
    else if (loadGameAtStartup && game->saveFilename.empty())
        LOG(ERROR, "sim: could not load %s", startupLoadPath.c_str());
    else
        code = Sim::run(*game, m_sim);
    while (!states.empty())
        popState();
    for (State *state : statesToDelete)
        delete state;
    statesToDelete.clear();
    window.close();
    running = false;
    return code;
}

/** Pushes the given State ontop of the state stack, causing it to receive events. */
void Application::pushState(State *state)
{
    assert(state != NULL && "pushState() requires non-NULL state");
    LOG(INFO, "pushing state %s", typeid(*state).name());
    if (!states.empty())
    {
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
    State *state = states.top();
    state->deactivate();
    states.pop();
    statesToDelete.push_back(state);
    if (!states.empty())
    {
        states.top()->activate();
    }
}

void Application::loadGame()
{
    if (states.empty())
        return;
    std::string filename;
    NativeFileDialog::Result result = NativeFileDialog::openTowerFile("Load Tower", userDataPath("default.tower"), filename);
    if (result == NativeFileDialog::Result::Selected)
    {
        loadGameFromFile(filename);
        return;
    }
    if (result == NativeFileDialog::Result::Cancelled)
        return;

    showFilenameDialog("Load Game", "default.tower", [this](const std::string &filename)
                       { loadGameFromFile(filename); });
}

bool Application::loadSettings()
{
    tinyxml2::XMLDocument xml;
    std::string userDir = getUserDataDir();
#ifdef _WIN32
    std::string path = userDir + "\\settings.xml";
#else
    std::string path = userDir + "/settings.xml";
#endif
    if (xml.LoadFile(path.c_str()) == 0)
    {
        tinyxml2::XMLElement *root = xml.RootElement();
        if (root && strcmp(root->Name(), "settings") == 0)
        {
            float valVol = 100.f;
            if (root->QueryFloatAttribute("volume", &valVol) == tinyxml2::XML_SUCCESS) {
                audioVolume = std::max(0.f, std::min(100.f, valVol));
            }
            root->QueryBoolAttribute("muted", &audioMuted);
            root->QueryBoolAttribute("fullscreen", &fullscreen);
            float valScale = 1.0f;
            if (root->QueryFloatAttribute("uiScale", &valScale) == tinyxml2::XML_SUCCESS) {
                uiScale = std::max(1.0f, std::min(2.0f, valScale));
            }
			const char *exe = root->Attribute("simTowerExecutable");
			simTowerExecutable = exe ? exe : "";
			const char *yoot = root->Attribute("yootPluginsPath");
			yootPluginsPath = yoot ? yoot : "";
            LOG(INFO, "Loaded settings: volume=%f, muted=%d, fullscreen=%d, uiScale=%f",
                audioVolume, audioMuted, fullscreen, uiScale);
            return true;
        }
    }
    return false;
}

void Application::saveSettings()
{
    std::string userDir = getUserDataDir();
    ensureDirExists(userDir);
#ifdef _WIN32
    std::string path = userDir + "\\settings.xml";
#else
    std::string path = userDir + "/settings.xml";
#endif
    FILE *f = fopen(path.c_str(), "w");
    if (!f)
    {
        LOG(ERROR, "Could not open settings file %s for writing", path.c_str());
        return;
    }
    tinyxml2::XMLPrinter xml(f);
    xml.OpenElement("settings");
    xml.PushAttribute("volume", audioVolume);
    xml.PushAttribute("muted", audioMuted);
    xml.PushAttribute("fullscreen", fullscreen);
    xml.PushAttribute("uiScale", uiScale);
    xml.PushAttribute("simTowerExecutable", simTowerExecutable.c_str());
    xml.PushAttribute("yootPluginsPath", yootPluginsPath.c_str());
    xml.CloseElement();
    fclose(f);
    LOG(INFO, "Saved settings: volume=%f, muted=%d, fullscreen=%d, uiScale=%f",
        audioVolume, audioMuted, fullscreen, uiScale);
}

void Application::setAudioVolume(float volume)
{
    audioVolume = std::max(0.f, std::min(100.f, volume));
    if (!states.empty())
    {
        Game *game = dynamic_cast<Game *>(states.top());
        if (game)
        {
            for (auto *snd : game->playingSounds)
            {
                if (!audioMuted)
                {
                    snd->setVolume(audioVolume);
                }
            }
        }
    }
}

void Application::setAudioMuted(bool mute)
{
    audioMuted = mute;
    if (!states.empty())
    {
        Game *game = dynamic_cast<Game *>(states.top());
        if (game)
        {
            for (auto *snd : game->playingSounds)
            {
                snd->setVolume(audioMuted ? 0.f : audioVolume);
            }
        }
    }
}

void Application::showSettingsDialog()
{
    auto existing = gui.get<tgui::ChildWindow>("SettingsDialog");
    if (existing)
    {
        existing->moveToFront();
        return;
    }

    float origVolume = audioVolume;
    bool origMuted = audioMuted;

    auto dialog = tgui::ChildWindow::create("Settings");
    dialog->setWidgetName("SettingsDialog");
    dialog->setClientSize({500 * uiScale, 445 * uiScale});
    dialog->getRenderer()->setTitleBarHeight(24 * uiScale);
    dialog->setTitleTextSize(14 * uiScale);
    dialog->getRenderer()->setTitleColor(sf::Color(220, 220, 220));
    dialog->getRenderer()->setBackgroundColor(sf::Color(30, 30, 35));
    dialog->getRenderer()->setTitleBarColor(sf::Color(45, 45, 50));
    dialog->getRenderer()->setBorderColor(sf::Color(80, 80, 90));
    dialog->getRenderer()->setBorders(2);
    dialog->setPosition({(window.getSize().x - dialog->getSize().x) / 2.f,
                         (window.getSize().y - dialog->getSize().y) / 2.f});
    dialog->setResizable(false);

    dialog->onClose([this, dialog, origVolume, origMuted]() {
        setAudioMuted(origMuted);
        setAudioVolume(origVolume);
        gui.remove(dialog);
    });

    float padding = 15.f * uiScale;
    float currentY = 15.f * uiScale;

    // --- AUDIO SECTION ---
    auto lblAudio = tgui::Label::create("AUDIO");
    lblAudio->setTextSize(14 * uiScale);
    lblAudio->getRenderer()->setTextColor(sf::Color(0, 150, 255));
    lblAudio->setPosition(padding, currentY);
    dialog->add(lblAudio);
    currentY += 25.f * uiScale;

    auto chkMute = tgui::CheckBox::create("Mute Sounds");
    chkMute->setTextSize(12 * uiScale);
    chkMute->getRenderer()->setTextColor(sf::Color(200, 200, 200));
    chkMute->setPosition(padding + 10.f * uiScale, currentY);
    chkMute->setChecked(audioMuted);
    dialog->add(chkMute);
    chkMute->onChange([this](bool checked) {
        setAudioMuted(checked);
    });
    currentY += 25.f * uiScale;

    auto lblVolume = tgui::Label::create("Volume: " + std::to_string(static_cast<int>(audioVolume)) + "%");
    lblVolume->setTextSize(12 * uiScale);
    lblVolume->getRenderer()->setTextColor(sf::Color(200, 200, 200));
    lblVolume->setPosition(padding + 10.f * uiScale, currentY);
    dialog->add(lblVolume);

    auto sldVolume = tgui::Slider::create(0.f, 100.f);
    sldVolume->setSize(180 * uiScale, 12 * uiScale);
    sldVolume->setPosition(padding + 150.f * uiScale, currentY + 2.f * uiScale);
    sldVolume->setValue(audioVolume);
    dialog->add(sldVolume);

    sldVolume->onValueChange([this, lblVolume](float val) {
        lblVolume->setText("Volume: " + std::to_string(static_cast<int>(val)) + "%");
        setAudioVolume(val);
    });
    currentY += 35.f * uiScale;

    // --- VIDEO SECTION ---
    auto lblVideo = tgui::Label::create("VIDEO & SCREEN");
    lblVideo->setTextSize(14 * uiScale);
    lblVideo->getRenderer()->setTextColor(sf::Color(0, 150, 255));
    lblVideo->setPosition(padding, currentY);
    dialog->add(lblVideo);
    currentY += 25.f * uiScale;

    auto chkFullscreen = tgui::CheckBox::create("Fullscreen Mode");
    chkFullscreen->setTextSize(12 * uiScale);
    chkFullscreen->getRenderer()->setTextColor(sf::Color(200, 200, 200));
    chkFullscreen->setPosition(padding + 10.f * uiScale, currentY);
    chkFullscreen->setChecked(fullscreen);
    dialog->add(chkFullscreen);
    currentY += 25.f * uiScale;

    auto lblScale = tgui::Label::create("UI Scale / Zoom:");
    lblScale->setTextSize(12 * uiScale);
    lblScale->getRenderer()->setTextColor(sf::Color(200, 200, 200));
    lblScale->setPosition(padding + 10.f * uiScale, currentY);
    dialog->add(lblScale);

    auto cmbScale = tgui::ComboBox::create();
    cmbScale->setSize(180 * uiScale, 20 * uiScale);
    cmbScale->setPosition(padding + 150.f * uiScale, currentY - 2.f * uiScale);
    cmbScale->addItem("1.0x (1280x768)");
    cmbScale->addItem("1.25x (1600x960)");
    cmbScale->addItem("1.5x (1920x1152)");
    cmbScale->addItem("1.75x (2240x1344)");
    cmbScale->addItem("2.0x (2560x1536)");

    if (std::abs(uiScale - 1.0f) < 0.1f) cmbScale->setSelectedItem("1.0x (1280x768)");
    else if (std::abs(uiScale - 1.25f) < 0.1f) cmbScale->setSelectedItem("1.25x (1600x960)");
    else if (std::abs(uiScale - 1.5f) < 0.1f) cmbScale->setSelectedItem("1.5x (1920x1152)");
    else if (std::abs(uiScale - 1.75f) < 0.1f) cmbScale->setSelectedItem("1.75x (2240x1344)");
    else if (std::abs(uiScale - 2.0f) < 0.1f) cmbScale->setSelectedItem("2.0x (2560x1536)");
    else cmbScale->setSelectedItem("1.0x (1280x768)");

    dialog->add(cmbScale);
    currentY += 45.f * uiScale;

    // --- SIMTOWER RESOURCES ---
    auto lblResources = tgui::Label::create("SIMTOWER RESOURCES");
    lblResources->setTextSize(14 * uiScale);
    lblResources->getRenderer()->setTextColor(sf::Color(0, 150, 255));
    lblResources->setPosition(padding, currentY);
    dialog->add(lblResources);
    currentY += 25.f * uiScale;

    auto pendingExecutable = std::make_shared<std::string>(simTowerExecutable);
    auto exePath = tgui::EditBox::create();
    exePath->setSize(310 * uiScale, 24 * uiScale);
    exePath->setPosition(padding + 10.f * uiScale, currentY);
    exePath->setText(simTowerExecutable.empty() ? "Automatic" : simTowerExecutable);
    exePath->setReadOnly(true);
    dialog->add(exePath);

    auto btnBrowseExecutable = tgui::Button::create("Browse...");
    btnBrowseExecutable->setSize(90 * uiScale, 24 * uiScale);
    btnBrowseExecutable->setPosition(padding + 330.f * uiScale, currentY);
    dialog->add(btnBrowseExecutable);
    btnBrowseExecutable->onPress([pendingExecutable, exePath]() {
        std::string selected;
        if (NativeFileDialog::openExecutableFile(
                "Choose SIMTOWER.EXE or SIMTOWER.EX_", selected) == NativeFileDialog::Result::Selected)
        {
            *pendingExecutable = selected;
            exePath->setText(selected);
        }
    });

    auto btnAutoExecutable = tgui::Button::create("Auto");
    btnAutoExecutable->setSize(50 * uiScale, 24 * uiScale);
    btnAutoExecutable->setPosition(padding + 425.f * uiScale, currentY);
    dialog->add(btnAutoExecutable);
    btnAutoExecutable->onPress([pendingExecutable, exePath]() {
        pendingExecutable->clear();
        exePath->setText("Automatic");
    });
    currentY += 28.f * uiScale;

    // --- YOOTTOWER RESOURCES ---
    auto lblYoot = tgui::Label::create("YOOTTOWER RESOURCES");
    lblYoot->setTextSize(14 * uiScale);
    lblYoot->getRenderer()->setTextColor(sf::Color(0, 150, 255));
    lblYoot->setPosition(padding, currentY);
    dialog->add(lblYoot);
    currentY += 25.f * uiScale;

    // Folder containing the official YootTower .t2p plugins. Independent
    // of the SIMTOWER.EXE pick so both editions' media can be provided at
    // once; the skin selector decides per tower which set is used.
    auto pendingYoot = std::make_shared<std::string>(yootPluginsPath);
    auto yootPath = tgui::EditBox::create();
    yootPath->setSize(310 * uiScale, 24 * uiScale);
    yootPath->setPosition(padding + 10.f * uiScale, currentY);
    yootPath->setText(yootPluginsPath.empty() ? "Automatic" : yootPluginsPath);
    yootPath->setReadOnly(true);
    dialog->add(yootPath);

    auto btnBrowseYoot = tgui::Button::create("Browse...");
    btnBrowseYoot->setSize(90 * uiScale, 24 * uiScale);
    btnBrowseYoot->setPosition(padding + 330.f * uiScale, currentY);
    dialog->add(btnBrowseYoot);
    btnBrowseYoot->onPress([pendingYoot, yootPath]() {
        std::string selected;
        if (NativeFileDialog::openDirectory(
                "Choose the folder containing YootTower .t2p plugins", selected) == NativeFileDialog::Result::Selected)
        {
            *pendingYoot = selected;
            yootPath->setText(selected);
        }
    });

    auto btnAutoYoot = tgui::Button::create("Auto");
    btnAutoYoot->setSize(50 * uiScale, 24 * uiScale);
    btnAutoYoot->setPosition(padding + 425.f * uiScale, currentY);
    dialog->add(btnAutoYoot);
    btnAutoYoot->onPress([pendingYoot, yootPath]() {
        pendingYoot->clear();
        yootPath->setText("Automatic");
    });
    currentY += 28.f * uiScale;

    auto lblRestart = tgui::Label::create("Resource sources are used after restarting OpenSkyscraper.");
    lblRestart->setTextSize(11 * uiScale);
    lblRestart->getRenderer()->setTextColor(sf::Color(170, 170, 170));
    lblRestart->setPosition(padding + 10.f * uiScale, currentY);
    dialog->add(lblRestart);
    currentY += 28.f * uiScale;

    // --- BUTTONS ---
    auto btnApply = tgui::Button::create("Apply");
    btnApply->setSize(100 * uiScale, 30 * uiScale);
    btnApply->setTextSize(14 * uiScale);
    btnApply->setPosition(70 * uiScale, dialog->getClientSize().y - 50 * uiScale);
    btnApply->getRenderer()->setBackgroundColor(sf::Color(0, 122, 204));
    btnApply->getRenderer()->setTextColor(sf::Color::White);
    btnApply->getRenderer()->setBackgroundColorHover(sf::Color(20, 142, 224));
    dialog->add(btnApply);

    auto btnCancel = tgui::Button::create("Cancel");
    btnCancel->setSize(100 * uiScale, 30 * uiScale);
    btnCancel->setTextSize(14 * uiScale);
    btnCancel->setPosition(dialog->getClientSize().x - 170 * uiScale, dialog->getClientSize().y - 50 * uiScale);
    btnCancel->getRenderer()->setBackgroundColor(sf::Color(60, 60, 65));
    btnCancel->getRenderer()->setTextColor(sf::Color::White);
    btnCancel->getRenderer()->setBackgroundColorHover(sf::Color(80, 80, 85));
    dialog->add(btnCancel);

    btnCancel->onPress([this, dialog, origVolume, origMuted]() {
        setAudioMuted(origMuted);
        setAudioVolume(origVolume);
        gui.remove(dialog);
    });

    btnApply->onPress([this, dialog, chkMute, sldVolume, chkFullscreen, cmbScale, pendingExecutable, pendingYoot]() {
        bool newMute = chkMute->isChecked();
        float newVolume = sldVolume->getValue();
        bool newFullscreen = chkFullscreen->isChecked();

        std::string selectedScale = cmbScale->getSelectedItem().toStdString();
        float newScale = 1.0f;
        if (selectedScale.find("1.25x") != std::string::npos) newScale = 1.25f;
        else if (selectedScale.find("1.5x") != std::string::npos) newScale = 1.5f;
        else if (selectedScale.find("1.75x") != std::string::npos) newScale = 1.75f;
        else if (selectedScale.find("2.0x") != std::string::npos) newScale = 2.0f;

        setAudioMuted(newMute);
        setAudioVolume(newVolume);
        simTowerExecutable = *pendingExecutable;
        yootPluginsPath = *pendingYoot;

        bool needWindowRecreation = (newFullscreen != fullscreen) || (std::abs(newScale - uiScale) > 0.01f);

        if (needWindowRecreation)
        {
            fullscreen = newFullscreen;
            uiScale = newScale;

            if (fullscreen)
            {
                videoMode = sf::VideoMode::getDesktopMode();
                window.create(videoMode, "OpenSkyscraper SFML", sf::Style::Default, sf::State::Fullscreen);
            }
            else
            {
                videoMode = sf::VideoMode({static_cast<unsigned>(1280 * uiScale), static_cast<unsigned>(768 * uiScale)});
                window.create(videoMode, "OpenSkyscraper SFML", sf::Style::Default, sf::State::Windowed);
            }
            window.setVerticalSyncEnabled(true);
            gui.setWindow(window);

            gui.removeAllWidgets();
            makeMenu();
            if (!states.empty())
            {
                states.top()->reloadGUI();
            }
        }
        else
        {
            gui.remove(dialog);
        }

        saveSettings();
    });

    gui.add(dialog);
}

bool Application::hasSkinAssets(Skin skin) const
{
    switch (skin)
    {
    case Skin::OpenSky:
        return true;
    case Skin::Classic:
        return m_loadedSimTowerResources ||
               !simTowerExecutable.empty() ||
               data.exists("SIMTOWER.EXE") || data.exists("simtower.exe") ||
               data.exists("SIMTOWER.EX_") || data.exists("simtower.ex_");
    case Skin::Yoot:
        // Sky, ground, slabs and sides come from the world plugin, lobbies
        // and empty floors from Floor.t2p (KFloor.t2p in Kegon); anything
        // else falls back to OpenSky art per key.
        return pluginManager.has(yootWorldPluginFile(selectedWorld())) &&
               pluginManager.has(YootWorldArt::floorPluginFor(selectedWorld()));
    }
    return true;
}

void Application::showMessageDialog(const std::string &title, const std::string &message)
{
    auto existing = gui.get<tgui::ChildWindow>("MessageDialog");
    if (existing)
    {
        gui.remove(existing);
    }

    auto dialog = tgui::ChildWindow::create(title);
    dialog->setWidgetName("MessageDialog");
    dialog->setClientSize({380.f * uiScale, 175.f * uiScale});
    dialog->getRenderer()->setTitleBarHeight(24.f * uiScale);
    dialog->setTitleTextSize(14.f * uiScale);
    dialog->getRenderer()->setTitleColor(sf::Color(220, 220, 220));
    dialog->getRenderer()->setBackgroundColor(sf::Color(26, 30, 40));
    dialog->getRenderer()->setTitleBarColor(sf::Color(40, 46, 58));
    dialog->getRenderer()->setBorderColor(sf::Color(70, 80, 100));
    dialog->getRenderer()->setBorders(2);
    dialog->setPosition({(window.getSize().x - dialog->getSize().x) / 2.f,
                         (window.getSize().y - dialog->getSize().y) / 2.f});
    dialog->setResizable(false);

    auto msgLabel = tgui::Label::create(message);
    msgLabel->setTextSize(12.f * uiScale);
    msgLabel->getRenderer()->setTextColor(sf::Color(220, 225, 235));
    msgLabel->setPosition(20.f * uiScale, 18.f * uiScale);
    msgLabel->setSize(340.f * uiScale, 85.f * uiScale);
    dialog->add(msgLabel);

    auto okBtn = tgui::Button::create("OK");
    okBtn->setSize(90.f * uiScale, 28.f * uiScale);
    okBtn->setTextSize(13.f * uiScale);
    okBtn->setPosition((dialog->getClientSize().x - okBtn->getSize().x) / 2.f,
                       dialog->getClientSize().y - 38.f * uiScale);
    auto r = okBtn->getRenderer();
    r->setBackgroundColor(sf::Color(28, 92, 168));
    r->setTextColor(sf::Color::White);
    r->setBackgroundColorHover(sf::Color(38, 110, 195));
    r->setBorderColor(sf::Color(55, 125, 215));
    r->setBorders(1);
    r->setRoundedBorderRadius(4.f * uiScale);
    okBtn->onPress([this, dialog]() {
        gui.remove(dialog);
    });
    dialog->add(okBtn);

    gui.add(dialog);
    dialog->moveToFront();
}

BuildRules Application::makeBuildRules(BuildRules::Ruleset ruleset, YootWorld::Id world)
{
    if (ruleset != BuildRules::Ruleset::YootTower)
        return BuildRules(ruleset);

    std::map<int, RulesProfile>::const_iterator cached = m_yootProfiles.find(static_cast<int>(world));
    if (cached != m_yootProfiles.end())
        return BuildRules(ruleset, cached->second);

    std::string error;
    const YootWorld w = loadYootWorld(pluginManager, world, &error);
    if (!w.fromPlugin)
        LOG(WARNING, "YootTower world %s: %s; using built-in constants",
            yootWorldDisplayName(world), error.c_str());
    else if (!error.empty())
        LOG(WARNING, "YootTower world %s: %s", yootWorldDisplayName(world), error.c_str());

    RulesProfile profile = RulesProfile::yootTower(w);
    // Item unlocks from plugin ATTR (mGameLevel + world mask). Prototypes
    // with no YootTower counterpart stay on the SimTower lock table.
    const std::map<std::string, YootUnlock> unlocks = resolveYootUnlocks(pluginManager, w.worldMask);
    for (std::map<std::string, YootUnlock>::const_iterator u = unlocks.begin(); u != unlocks.end(); ++u)
    {
        RulesProfile::Unlock unlock;
        unlock.available = u->second.available;
        unlock.minRating = u->second.minGrade > 0 ? u->second.minGrade - 1 : 0;
        unlock.secret = u->second.secret;
        profile.unlocks[u->first] = unlock;
    }
    for (size_t i = 0; i < profile.vips.size(); ++i)
        profile.vips[i].destPrototypes = yootPrototypesForPluginId(pluginManager, profile.vips[i].destPluginId);

    LOG(INFO, "YootTower world %s: funds $%d, start %d stars, floors %d..%d, x [%d, %d), lobby %d, %zu grade rows, %zu VIPs, %zu unlocks%s",
        profile.worldName.c_str(), profile.startFunds, profile.startRating + 1,
        profile.lowestFloor, profile.highestFloor, profile.buildMinX, profile.buildMaxX,
        profile.lobbyHeight, profile.grades.size(), profile.vips.size(), profile.unlocks.size(),
        profile.unbuildable.empty() ? "" : ", FINF mask");
    m_yootProfiles[static_cast<int>(world)] = profile;
    return BuildRules(ruleset, profile);
}

void Application::showNewTowerDialog()
{
    auto existing = gui.get<tgui::ChildWindow>("NewTowerDialog");
    if (existing)
    {
        existing->moveToFront();
        return;
    }

    Game* currentGame = !states.empty() ? dynamic_cast<Game*>(states.top()) : nullptr;

    auto dialog = tgui::ChildWindow::create("New Tower");
    dialog->setWidgetName("NewTowerDialog");
    const float dialogH = (currentGame ? 394.f : 344.f) * uiScale;
    dialog->setClientSize({420 * uiScale, dialogH});
    dialog->getRenderer()->setTitleBarHeight(24 * uiScale);
    dialog->setTitleTextSize(14 * uiScale);
    dialog->getRenderer()->setTitleColor(sf::Color(220, 220, 220));
    dialog->getRenderer()->setBackgroundColor(sf::Color(24, 28, 36));
    dialog->getRenderer()->setTitleBarColor(sf::Color(40, 46, 58));
    dialog->getRenderer()->setBorderColor(sf::Color(70, 80, 100));
    dialog->getRenderer()->setBorders(2);
    dialog->setPosition({(window.getSize().x - dialog->getSize().x) / 2.f,
                         (window.getSize().y - dialog->getSize().y) / 2.f});
    dialog->setResizable(false);

    float padding = 20.f * uiScale;
    float currentY = 16.f * uiScale;

    if (currentGame)
    {
        auto warnLabel = tgui::Label::create("Warning: Starting a new tower will discard all unsaved progress in the current game!");
        warnLabel->setTextSize(11 * uiScale);
        warnLabel->getRenderer()->setTextColor(sf::Color(255, 185, 75)); // Amber warning
        warnLabel->setSize({dialog->getClientSize().x - 2 * padding, 36 * uiScale});
        warnLabel->setPosition(padding, currentY);
        dialog->add(warnLabel);
        currentY += 44.f * uiScale;
    }

    if (!isSkinSanctioned(selectedRuleset(), selectedSkin()))
    {
        Skin def = defaultSkinForRuleset(selectedRuleset());
        setSkinOverride(hasSkinAssets(def) ? def : Skin::OpenSky);
    }

    // Ruleset (Game Mode)
    auto modeBtn = tgui::Button::create(std::string("Game Mode: ") + BuildRules(selectedRuleset()).displayName());
    modeBtn->setSize(dialog->getClientSize().x - 2 * padding, 34 * uiScale);
    modeBtn->setTextSize(13 * uiScale);
    modeBtn->setPosition(padding, currentY);
    auto modeR = modeBtn->getRenderer();
    modeR->setBackgroundColor(sf::Color(42, 48, 62));
    modeR->setTextColor(sf::Color(230, 235, 245));
    modeR->setBackgroundColorHover(sf::Color(55, 64, 82));
    modeR->setBorderColor(sf::Color(65, 76, 98));
    modeR->setBorders(1);
    modeR->setRoundedBorderRadius(4 * uiScale);
    dialog->add(modeBtn);
    currentY += 44.f * uiScale;

    // World (YootTower mode only)
    auto worldBtn = tgui::Button::create(std::string("World: ") + yootWorldDisplayName(selectedWorld()));
    worldBtn->setSize(dialog->getClientSize().x - 2 * padding, 34 * uiScale);
    worldBtn->setTextSize(13 * uiScale);
    worldBtn->setPosition(padding, currentY);
    auto worldR = worldBtn->getRenderer();
    worldR->setBackgroundColor(sf::Color(42, 48, 62));
    worldR->setTextColor(sf::Color(230, 235, 245));
    worldR->setBackgroundColorHover(sf::Color(55, 64, 82));
    worldR->setBorderColor(sf::Color(65, 76, 98));
    worldR->setBorders(1);
    worldR->setRoundedBorderRadius(4 * uiScale);
    worldBtn->setEnabled(selectedRuleset() == BuildRules::Ruleset::YootTower);
    dialog->add(worldBtn);
    currentY += 44.f * uiScale;

    // Skin selector button
    auto skinBtn = tgui::Button::create();
    skinBtn->setSize(dialog->getClientSize().x - 2 * padding, 34 * uiScale);
    skinBtn->setTextSize(13 * uiScale);
    skinBtn->setPosition(padding, currentY);
    auto skinR = skinBtn->getRenderer();
    skinR->setBackgroundColor(sf::Color(42, 48, 62));
    skinR->setTextColor(sf::Color(230, 235, 245));
    skinR->setBackgroundColorHover(sf::Color(55, 64, 82));
    skinR->setBorderColor(sf::Color(65, 76, 98));
    skinR->setBorders(1);
    skinR->setRoundedBorderRadius(4 * uiScale);
    dialog->add(skinBtn);
    currentY += 40.f * uiScale;

    // Warning note for gated/incomplete skins
    auto warnSkinLabel = tgui::Label::create();
    warnSkinLabel->setTextSize(10 * uiScale);
    warnSkinLabel->getRenderer()->setTextColor(sf::Color(255, 195, 80));
    warnSkinLabel->setSize({dialog->getClientSize().x - 2 * padding, 30 * uiScale});
    warnSkinLabel->setPosition(padding, currentY);
    dialog->add(warnSkinLabel);

    // Start New Tower Button
    auto startBtn = tgui::Button::create("Start New Tower");
    startBtn->setSize(160 * uiScale, 34 * uiScale);
    startBtn->setTextSize(13 * uiScale);
    startBtn->setPosition(padding, dialog->getClientSize().y - 46 * uiScale);
    dialog->add(startBtn);

    // Cancel Button
    auto cancelBtn = tgui::Button::create("Cancel");
    cancelBtn->setSize(100 * uiScale, 34 * uiScale);
    cancelBtn->setTextSize(13 * uiScale);
    cancelBtn->setPosition(dialog->getClientSize().x - padding - 100 * uiScale, dialog->getClientSize().y - 46 * uiScale);
    auto cancelR = cancelBtn->getRenderer();
    cancelR->setBackgroundColor(sf::Color(55, 60, 72));
    cancelR->setTextColor(sf::Color::White);
    cancelR->setBackgroundColorHover(sf::Color(70, 78, 92));
    cancelR->setBorderColor(sf::Color(75, 84, 100));
    cancelR->setBorders(1);
    cancelR->setRoundedBorderRadius(4 * uiScale);
    dialog->add(cancelBtn);

    auto updateSkinState = [this, skinBtn, startBtn, warnSkinLabel]() {
        bool available = hasSkinAssets(selectedSkin());
        if (available) {
            skinBtn->setText(std::string("Skin: ") + skinDisplayName(selectedSkin()));
            skinBtn->getRenderer()->setBorderColor(sf::Color(65, 76, 98));
            startBtn->setEnabled(true);
            startBtn->setText("Start New Tower");
            auto r = startBtn->getRenderer();
            r->setBackgroundColor(sf::Color(28, 92, 168));
            r->setTextColor(sf::Color::White);
            r->setBackgroundColorHover(sf::Color(38, 110, 195));
            r->setBorderColor(sf::Color(50, 120, 210));
            r->setBorders(1);
            r->setRoundedBorderRadius(4 * uiScale);
            warnSkinLabel->setVisible(false);
        } else {
            skinBtn->setText(std::string("🔒 Skin: ") + skinDisplayName(selectedSkin()) + " (Locked)");
            skinBtn->getRenderer()->setBorderColor(sf::Color(190, 110, 45));
            startBtn->setEnabled(false);
            startBtn->setText("Skin Unavailable (Locked)");
            auto r = startBtn->getRenderer();
            r->setBackgroundColor(sf::Color(48, 52, 60));
            r->setTextColor(sf::Color(140, 145, 155));
            r->setBackgroundColorHover(sf::Color(48, 52, 60));
            r->setBorderColor(sf::Color(70, 75, 85));
            r->setBorders(1);
            r->setRoundedBorderRadius(4 * uiScale);
            if (selectedSkin() == Skin::Yoot)
                warnSkinLabel->setText(std::string("⚠️ YootTower skin needs ") + yootWorldPluginFile(selectedWorld()) +
                                       " and " + YootWorldArt::floorPluginFor(selectedWorld()) + ". Select OpenSky.");
            else
                warnSkinLabel->setText("⚠️ SIMTOWER.EXE not found. Select OpenSky or provide in Settings.");
            warnSkinLabel->setVisible(true);
        }
    };
    updateSkinState();

    modeBtn->onPress([this, modeBtn, worldBtn, updateSkinState] {
        BuildRules::Ruleset next = static_cast<BuildRules::Ruleset>((static_cast<int>(selectedRuleset()) + 1) % 3);
        setRulesetOverride(next);
        modeBtn->setText(std::string("Game Mode: ") + BuildRules(next).displayName());
        worldBtn->setEnabled(next == BuildRules::Ruleset::YootTower);
        updateSkinState();
    });

    worldBtn->onPress([this, worldBtn, updateSkinState] {
        YootWorld::Id next = static_cast<YootWorld::Id>((static_cast<int>(selectedWorld()) + 1) % 3);
        setWorldOverride(next);
        worldBtn->setText(std::string("World: ") + yootWorldDisplayName(next));
        updateSkinState(); // Kegon needs KFloor.t2p
    });

    skinBtn->onPress([this, updateSkinState] {
        std::size_t next = 0;
        const std::size_t count = sanctionedSkinCount(selectedRuleset());
        for (std::size_t i = 0; i < count; i++) {
            if (sanctionedSkin(selectedRuleset(), i) == selectedSkin())
                next = (i + 1) % count;
        }
        Skin s = sanctionedSkin(selectedRuleset(), next);
        setSkinOverride(s);
        updateSkinState();
    });

    startBtn->onPress([this, dialog]() {
        if (!hasSkinAssets(selectedSkin()))
        {
            if (selectedSkin() == Skin::Yoot)
            {
                showMessageDialog("Skin Unavailable",
                    std::string("The YootTower skin draws its sky, ground and lobby from the YootTower plugins ") +
                    yootWorldPluginFile(selectedWorld()) + " and " + YootWorldArt::floorPluginFor(selectedWorld()) +
                    ", which were not found.\n\nAdd the YootTower Plugins folder in Settings or select the OpenSky skin.");
            }
            else if (selectedSkin() == Skin::Classic)
            {
                showMessageDialog("SIMTOWER.EXE Required",
                    "The Classic skin requires SIMTOWER.EXE to load original assets.\n\nPlease configure SIMTOWER.EXE in Settings or select the OpenSky skin.");
            }
            return;
        }
        gui.remove(dialog);
        Game* newGame = new Game(*this);
        newGame->seedNewTower();
        newGame->saveFilename = "";
        newGame->isDirty = false;
        popState();
        pushState(newGame);
    });

    cancelBtn->onPress([this, dialog]() {
        gui.remove(dialog);
    });

    dialog->onClose([this, dialog]() {
        gui.remove(dialog);
    });

    gui.add(dialog);
}
