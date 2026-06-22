#include "MainMenu.h"
#include "Game.h"

using namespace OT;

MainMenu::MainMenu(Application &app)
    : State("menu"),
      app(app)
{
    reloadGUI();
}

void MainMenu::activate()
{
    State::activate();
}

void MainMenu::deactivate()
{
    State::deactivate();
    if (panel) {
        panel->removeAllWidgets();
    }
    app.gui.remove(bgPicture);
    app.gui.remove(panel);
}

bool MainMenu::handleEvent(sf::Event &event)
{
    return false;
}

void MainMenu::advance(double dt)
{
}

void MainMenu::reloadGUI()
{
    if (bgPicture) app.gui.remove(bgPicture);
    if (panel) app.gui.remove(panel);

    sf::Vector2u windowSize = app.window.getSize();
    sf::Vector2f panelSize{320.f * app.uiScale, 180.f * app.uiScale};

    tgui::Texture bgTx = tgui::Texture();
    {
        const sf::Texture &bgTex = app.bitmaps["simtower/ui/menubg"];
        bgTx.loadFromPixelData(bgTex.getSize(), bgTex.copyToImage().getPixelsPtr());
    }
    bgPicture = tgui::Picture::create(bgTx);
    bgPicture->setSize({static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)});
    bgPicture->setPosition({0, 0});
    app.gui.add(bgPicture);

    panel = tgui::Panel::create({panelSize.x, panelSize.y});
    panel->setPosition({(windowSize.x / 2.f) - (panelSize.x / 2.f), (windowSize.y / 2.f) - (panelSize.y / 2.f)});
    
    // Modern panel styling
    auto panelRenderer = panel->getRenderer();
    panelRenderer->setBackgroundColor(sf::Color(30, 30, 35, 220)); // Semi-transparent dark background
    panelRenderer->setBorderColor(sf::Color(0, 122, 204)); // Blue accents
    panelRenderer->setBorders(tgui::Outline(2 * app.uiScale));
    panelRenderer->setRoundedBorderRadius(8 * app.uiScale);
    app.gui.add(panel);

    auto layout = tgui::VerticalLayout::create();
    layout->setOrigin(0.5f, 0.5f);
    layout->setPosition("50%", "50%");
    panel->add(layout);
    layout->setSize("90%", "90%");

    newButton = tgui::Button::create("New Tower");
    newButton->setTextSize(16 * app.uiScale);
    newButton->onPress(&MainMenu::onNewGamePress, this);
    layout->add(newButton);

    layout->addSpace(0.3f);

    loadButton = tgui::Button::create("Load Saved Tower");
    loadButton->setTextSize(16 * app.uiScale);
    loadButton->onPress([this]() { app.loadGame(); });
    layout->add(loadButton);

    layout->addSpace(0.3f);

    settingsButton = tgui::Button::create("Settings");
    settingsButton->setTextSize(16 * app.uiScale);
    settingsButton->onPress([this]() { app.showSettingsDialog(); });
    layout->add(settingsButton);

    layout->addSpace(0.3f);

    quitButton = tgui::Button::create("Quit");
    quitButton->setTextSize(16 * app.uiScale);
    quitButton->onPress([this]() { app.window.close(); });
    layout->add(quitButton);

    // Style buttons modernly
    auto styleBtn = [this](tgui::Button::Ptr btn) {
        auto r = btn->getRenderer();
        r->setBackgroundColor(sf::Color(45, 45, 50));
        r->setTextColor(sf::Color(240, 240, 240));
        r->setBackgroundColorHover(sf::Color(60, 60, 65));
        r->setTextColorHover(sf::Color(255, 255, 255));
        r->setBackgroundColorDown(sf::Color(0, 122, 204));
        r->setTextColorDown(sf::Color::White);
        r->setBorderColor(sf::Color(70, 70, 75));
        r->setBorders(1);
        r->setRoundedBorderRadius(4 * app.uiScale);
    };

    styleBtn(newButton);
    styleBtn(loadButton);
    styleBtn(settingsButton);
    styleBtn(quitButton);
}

void MainMenu::onNewGamePress()
{
    Game *game = new Game(app);
    game->seedNewTower();
    game->saveFilename = "";
    game->isDirty = false;
    app.popState();
    app.pushState(game);
}
