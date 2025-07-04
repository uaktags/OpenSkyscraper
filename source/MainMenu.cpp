#include "MainMenu.h"
#include "Game.h"

using namespace OT;

MainMenu::MainMenu(Application & app) 
:   State("menu"),
    app(app)
{
    sf::Vector2 panelSize {300 * app.uiScale, 120 * app.uiScale};
    sf::Vector2u windowSize = app.window.getSize();
    
    tgui::Texture bgTx = tgui::Texture();
    bgTx.load(app.bitmaps["simtower/ui/menubg"]);
    //bgTx.load(app.bitmaps["menubg.png"]);
    bgPicture = tgui::Picture::create(bgTx);
    bgPicture->setPosition({(windowSize.x / 2) - (bgTx.getImageSize().x / 2) * app.uiScale, (windowSize.y / 2) - (bgTx.getImageSize().y / 2) * app.uiScale});
    app.gui.add(bgPicture);

    panel = tgui::Panel::create({panelSize.x, panelSize.y});
    
    panel->setPosition({(windowSize.x / 2) - (panelSize.x / 2), (windowSize.y / 2) - (panelSize.y / 2)});
    panel->getRenderer()->setBorders(tgui::Outline(2 * app.uiScale));
    app.gui.add(panel);

    auto layout = tgui::VerticalLayout::create();
    layout->setOrigin(.5f, .5f);
    layout->setPosition("50%", "50%");
    panel->add(layout);
    layout->setSize("95%", "95%");

    newButton = tgui::Button::create("New Tower");
    newButton->setTextSize(18 * app.uiScale);
    newButton->onPress(&MainMenu::onNewGamePress, this);
    layout->add(newButton);

    layout->addSpace(0.4f);

    loadButton = tgui::Button::create("Load Saved Tower");
    loadButton->setTextSize(18 * app.uiScale);
    loadButton->onPress([this, &app](){
        app.loadGame();
    });
    layout->add(loadButton);

    layout->addSpace(0.4f);

    quitButton = tgui::Button::create("Quit");
    quitButton->setTextSize(18 * app.uiScale);
    quitButton->onPress([this, &app](){
        app.window.close();
    });
    layout->add(quitButton);
}

void MainMenu::activate() {
    State::activate();
}

void MainMenu::deactivate() {
    State::deactivate();
    panel->removeAllWidgets();
    app.gui.remove(bgPicture);
    app.gui.remove(panel);
}

bool MainMenu::handleEvent(sf::Event & event) {
    return false;
}

void MainMenu::advance(double dt) {
}

void MainMenu::onNewGamePress() {
    Game * game = new Game(app);
    game->saveFilename = "";
    game->isDirty = false;
    app.popState();
    app.pushState(game);
}