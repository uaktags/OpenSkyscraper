#include "MainMenu.h"
#include "Game.h"

using namespace OT;

MainMenu::MainMenu(Application & app) 
:   State("menu"),
    app(app)
{
    sf::Vector2 panelSize {300, 105};
    sf::Vector2u windowSize = app.window.getSize();
    
    tgui::Texture bgTx = tgui::Texture();
    bgTx.load(app.bitmaps["simtower/ui/menubg"]);
    bgPicture = tgui::Picture::create(bgTx);
    bgPicture->setPosition({(windowSize.x / 2) - (bgTx.getImageSize().x / 2), (windowSize.y / 2) - (bgTx.getImageSize().y / 2)});
    app.gui.add(bgPicture);

    panel = tgui::Panel::create({panelSize.x, panelSize.y});
    
    panel->setPosition({(windowSize.x / 2) - (panelSize.x / 2), (windowSize.y / 2) - (panelSize.y / 2)});
    panel->getRenderer()->setBorders(tgui::Outline(2));
    app.gui.add(panel);

    newButton = tgui::Button::create("New Tower");
    newButton->setSize({280, 20});
    newButton->setPosition({10, 5});
    newButton->onPress(&MainMenu::onNewGamePress, this);
    panel->add(newButton);

    loadButton = tgui::Button::create("Load Saved Tower");
    loadButton->setSize({280, 20});
    loadButton->setPosition({10, 40});
    panel->add(loadButton);

    quitButton = tgui::Button::create("Quit");
    quitButton->setSize({280, 20});
    quitButton->setPosition({10, 75});
    panel->add(quitButton);
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
    app.popState();
	app.pushState(game);
}