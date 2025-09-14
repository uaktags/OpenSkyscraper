#include "MainMenu.h"
#include "Game.h"

using namespace OT;

MainMenu::MainMenu(Application & app) 
:   State("menu"),
    app(app)
{
    sf::Vector2f panelSize(300 * app.guiManager.getUIScale(), 120 * app.guiManager.getUIScale());
    sf::Vector2u windowSize = app.window.getSize();
    
    // tgui::Texture bgTx(app.bitmaps["menubg.png"]);
    // bgPicture = tgui::Picture::create(bgTx);
    // // Scale background to fill window
    // bgPicture->setSize({static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)});
    // bgPicture->setPosition({0, 0});
    // app.guiManager.getBackend()->getGui()->add(bgPicture);

    panel = tgui::Panel::create(tgui::Layout2d(panelSize.x, panelSize.y));
    
    panel->setPosition(tgui::Layout2d((windowSize.x / 2) - (panelSize.x / 2), (windowSize.y / 2) - (panelSize.y / 2)));
    panel->getRenderer()->setBorders(tgui::Outline(2 * app.guiManager.getUIScale()));
    app.guiManager.getBackend()->getGui()->add(panel);

    auto layout = tgui::VerticalLayout::create();
    layout->setOrigin(.5f, .5f);
    layout->setPosition("50%", "50%");
    panel->add(layout);
    layout->setSize("95%", "95%");

    newButton = tgui::Button::create("New Tower");
    newButton->setTextSize(18 * app.guiManager.getUIScale());
    newButton->onPress(&MainMenu::onNewGamePress, this);
    layout->add(newButton);

    layout->addSpace(0.4f);

    loadButton = tgui::Button::create("Load Saved Tower");
    loadButton->setTextSize(18 * app.guiManager.getUIScale());
    loadButton->onPress([this, &app](){
        app.loadGame();
    });
    layout->add(loadButton);

    layout->addSpace(0.4f);

    quitButton = tgui::Button::create("Quit");
    quitButton->setTextSize(18 * app.guiManager.getUIScale());
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
    // app.guiManager.getBackend()->getGui()->remove(bgPicture);
    app.guiManager.getBackend()->getGui()->remove(panel);
}

bool MainMenu::handleEvent(sf::Event & event) {
    return false;
}

void MainMenu::advance(double dt) {
}

void MainMenu::draw(sf::RenderWindow& window) {
}

void MainMenu::onNewGamePress() {
    Game * game = new Game(app);
    game->saveFilename = "";
    game->isDirty = false;
    app.popState();
    app.pushState(game);
}