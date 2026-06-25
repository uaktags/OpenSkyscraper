/* Copyright (c) 2012-2015 Fabian Schuiki */
#include <TGUI/Widgets/MenuBar.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
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
#include <algorithm>
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
#include "NativeFileDialog.h"
#include "SimTowerLoader.h"
#include "OpenGL.h"
#include "tinyxml2.h"

using namespace OT;
using namespace std;

Application *OT::App = NULL;

Application::Application(int argc, char *argv[])
    : data(this),
      fonts(this),
      bitmaps(this),
      sounds(this)
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
    FILE *f = fopen(outPath.c_str(), "w");
    if (!f)
    {
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
        if (xml.LoadFile(path.c_str()) == 0)
        {
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
    editBox->getRenderer()->setRoundedBorderRadius(4 * uiScale);
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
                // Start a new game (same as MainMenu New Tower)
                Game* game = new Game(*this);
                game->seedNewTower();
                popState();
                pushState(game);
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

    SimTowerLoader *simtower = new SimTowerLoader(this);
    const bool loadedSimTowerResources = simtower->load();
    if (!loadedSimTowerResources)
    {
        sf::Texture skyProbe;
        sf::Texture toolboxProbe;
        sf::SoundBuffer cockProbe;
        const bool haveDumpedResources =
            bitmaps.load("simtower/sky", skyProbe) &&
            bitmaps.load("simtower/ui/toolbox/tools", toolboxProbe) &&
            sounds.load("simtower/cock", cockProbe);
        if (!haveDumpedResources)
        {
            LOG(ERROR, "unable to load SimTower resources");
            LOG(ERROR, "place SIMTOWER.EXE in the data directory or provide dumped bitmaps/sounds under data/bitmaps and data/sounds");
            delete simtower;
            exitCode = 1;
            return;
        }
        if (bitmaps.usedFallback || sounds.usedFallback)
            LOG(WARNING, "using synthetic placeholder resources because SimTower resources are unavailable");
        else
            LOG(WARNING, "using dumped SimTower resources from the data directory");
    }
    if (dumpResources)
    {
        if (loadedSimTowerResources)
        {
            simtower->dump(dumpResourcesPath);
        }
        else
        {
            LOG(WARNING, "resource dumping requires SIMTOWER.EXE or SIMTOWER.EX_");
        }
        delete simtower;
        simtower = NULL;
        return;
    }
    delete simtower;
    simtower = NULL;
    // exitCode = 1;

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
        if (setViewportScrollAtStartup)
            game->qaSetViewportScrollFractions(startupViewportScrollHorizontal, startupViewportScrollVertical);
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
    dialog->setClientSize({400 * uiScale, 300 * uiScale});
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

    btnApply->onPress([this, dialog, chkMute, sldVolume, chkFullscreen, cmbScale]() {
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
