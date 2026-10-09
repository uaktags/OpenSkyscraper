#include "MainMenu.h"
#include "YootWorldArt.h"
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
    sf::Vector2f panelSize{370.f * app.uiScale, 430.f * app.uiScale};

    tgui::Texture bgTx = tgui::Texture();
    {
        const sf::Texture &bgTex = app.bitmaps["splashscreen"];
        bgTx.loadFromPixelData(bgTex.getSize(), bgTex.copyToImage().getPixelsPtr());
    }
    bgPicture = tgui::Picture::create(bgTx);
    bgPicture->setSize({static_cast<float>(windowSize.x), static_cast<float>(windowSize.y)});
    bgPicture->setPosition({0, 0});
    app.gui.add(bgPicture);

    panel = tgui::Panel::create({panelSize.x, panelSize.y});
    panel->setPosition({(windowSize.x / 2.f) - (panelSize.x / 2.f), (windowSize.y / 2.f) - (panelSize.y / 2.f)});
    
    // Refined slate dark glass panel styling
    auto panelRenderer = panel->getRenderer();
    panelRenderer->setBackgroundColor(sf::Color(22, 26, 35, 240));
    panelRenderer->setBorderColor(sf::Color(65, 75, 95));
    panelRenderer->setBorders(tgui::Outline(1.5f * app.uiScale));
    panelRenderer->setRoundedBorderRadius(10 * app.uiScale);
    app.gui.add(panel);

    auto layout = tgui::VerticalLayout::create();
    layout->setOrigin(0.5f, 0.5f);
    layout->setPosition("50%", "50%");
    panel->add(layout);
    layout->setSize("90%", "92%");

    auto titleLabel = tgui::Label::create("OPENSKY");
    titleLabel->setTextSize(19 * app.uiScale);
    titleLabel->getRenderer()->setTextColor(sf::Color(240, 244, 252));
    titleLabel->getRenderer()->setTextStyle(tgui::TextStyle::Bold);
    titleLabel->setSize("100%", 24 * app.uiScale);
    titleLabel->setHorizontalAlignment(tgui::HorizontalAlignment::Center);
    layout->add(titleLabel);

    auto subLabel = tgui::Label::create("TOWER SIMULATION");
    subLabel->setTextSize(10 * app.uiScale);
    subLabel->getRenderer()->setTextColor(sf::Color(135, 150, 175));
    subLabel->setSize("100%", 14 * app.uiScale);
    subLabel->setHorizontalAlignment(tgui::HorizontalAlignment::Center);
    layout->add(subLabel);

    layout->addSpace(0.2f);

    newButton = tgui::Button::create("New Tower");
    newButton->setTextSize(15 * app.uiScale);
    newButton->onPress(&MainMenu::onNewGamePress, this);
    layout->add(newButton);

    layout->addSpace(0.2f);

    if (!isSkinSanctioned(app.selectedRuleset(), app.selectedSkin()))
    {
        Skin def = defaultSkinForRuleset(app.selectedRuleset());
        app.setSkinOverride(app.hasSkinAssets(def) ? def : Skin::OpenSky);
    }

    // Game-mode selector
    modeButton = tgui::Button::create(std::string("Mode: ") + BuildRules(app.selectedRuleset()).displayName());
    modeButton->setTextSize(13 * app.uiScale);
    layout->add(modeButton);

    layout->addSpace(0.2f);

    skinButton = tgui::Button::create();
    skinButton->setTextSize(13 * app.uiScale);
    layout->add(skinButton);

    skinWarningLabel = tgui::Label::create();
    skinWarningLabel->setTextSize(10 * app.uiScale);
    skinWarningLabel->getRenderer()->setTextColor(sf::Color(255, 195, 80));
    skinWarningLabel->setHorizontalAlignment(tgui::HorizontalAlignment::Center);
    skinWarningLabel->setSize("100%", 28 * app.uiScale);
    layout->add(skinWarningLabel);

    layout->addSpace(0.1f);

    loadButton = tgui::Button::create("Load Saved Tower");
    loadButton->setTextSize(13 * app.uiScale);
    loadButton->onPress([this]() { app.loadGame(); });
    layout->add(loadButton);

    layout->addSpace(0.2f);

    settingsButton = tgui::Button::create("Settings");
    settingsButton->setTextSize(13 * app.uiScale);
    settingsButton->onPress([this]() { app.showSettingsDialog(); });
    layout->add(settingsButton);

    layout->addSpace(0.2f);

    quitButton = tgui::Button::create("Quit");
    quitButton->setTextSize(13 * app.uiScale);
    quitButton->onPress([this]() { app.window.close(); });
    layout->add(quitButton);

    // Style option toggle buttons (Mode, Skin)
    auto styleOptionBtn = [this](tgui::Button::Ptr btn) {
        auto r = btn->getRenderer();
        r->setBackgroundColor(sf::Color(36, 42, 56));
        r->setTextColor(sf::Color(220, 230, 245));
        r->setBackgroundColorHover(sf::Color(48, 56, 74));
        r->setTextColorHover(sf::Color::White);
        r->setBorderColor(sf::Color(60, 70, 92));
        r->setBorders(1);
        r->setRoundedBorderRadius(5 * app.uiScale);
    };
    styleOptionBtn(modeButton);
    styleOptionBtn(skinButton);

    auto updateSkinAndStartState = [this]() {
        bool available = app.hasSkinAssets(app.selectedSkin());
        if (available) {
            skinButton->setText(std::string("Skin: ") + skinDisplayName(app.selectedSkin()));
            skinButton->getRenderer()->setBorderColor(sf::Color(60, 70, 92));
            newButton->setEnabled(true);
            newButton->setText("New Tower");
            auto r = newButton->getRenderer();
            r->setBackgroundColor(sf::Color(28, 92, 168));
            r->setTextColor(sf::Color::White);
            r->setBackgroundColorHover(sf::Color(38, 110, 195));
            r->setTextColorHover(sf::Color::White);
            r->setBorderColor(sf::Color(55, 125, 215));
            r->setBorders(1);
            r->setRoundedBorderRadius(5 * app.uiScale);
            if (skinWarningLabel) skinWarningLabel->setVisible(false);
        } else {
            skinButton->setText(std::string("🔒 Skin: ") + skinDisplayName(app.selectedSkin()) + " (Locked)");
            skinButton->getRenderer()->setBorderColor(sf::Color(190, 110, 45));
            newButton->setEnabled(false);
            newButton->setText("Skin Unavailable (Locked)");
            auto r = newButton->getRenderer();
            r->setBackgroundColor(sf::Color(45, 48, 56));
            r->setTextColor(sf::Color(140, 145, 155));
            r->setBackgroundColorHover(sf::Color(45, 48, 56));
            r->setTextColorHover(sf::Color(140, 145, 155));
            r->setBorderColor(sf::Color(65, 70, 82));
            r->setBorders(1);
            r->setRoundedBorderRadius(5 * app.uiScale);
            if (skinWarningLabel) {
                if (app.selectedSkin() == Skin::Yoot)
                    skinWarningLabel->setText(std::string("⚠️ YootTower skin needs ") + yootWorldPluginFile(app.selectedWorld()) +
                                              " and " + YootWorldArt::floorPluginFor(app.selectedWorld()) + ". Select OpenSky.");
                else
                    skinWarningLabel->setText("⚠️ SIMTOWER.EXE not found. Select OpenSky or provide in Settings.");
                skinWarningLabel->setVisible(true);
            }
        }
    };
    updateSkinAndStartState();

    modeButton->onPress([this, updateSkinAndStartState] {
        BuildRules::Ruleset next = static_cast<BuildRules::Ruleset>((static_cast<int>(app.selectedRuleset()) + 1) % 3);
        app.setRulesetOverride(next);
        modeButton->setText(std::string("Mode: ") + BuildRules(next).displayName());
        updateSkinAndStartState();
    });

    skinButton->onPress([this, updateSkinAndStartState] {
        std::size_t next = 0;
        const std::size_t count = sanctionedSkinCount(app.selectedRuleset());
        for (std::size_t i = 0; i < count; i++) {
            if (sanctionedSkin(app.selectedRuleset(), i) == app.selectedSkin())
                next = (i + 1) % count;
        }
        Skin s = sanctionedSkin(app.selectedRuleset(), next);
        app.setSkinOverride(s);
        updateSkinAndStartState();
    });

    // Style secondary buttons (Load, Settings)
    auto styleSecondaryBtn = [this](tgui::Button::Ptr btn) {
        auto r = btn->getRenderer();
        r->setBackgroundColor(sf::Color(42, 46, 58));
        r->setTextColor(sf::Color(225, 230, 240));
        r->setBackgroundColorHover(sf::Color(55, 62, 78));
        r->setTextColorHover(sf::Color::White);
        r->setBorderColor(sf::Color(68, 75, 94));
        r->setBorders(1);
        r->setRoundedBorderRadius(5 * app.uiScale);
    };
    styleSecondaryBtn(loadButton);
    styleSecondaryBtn(settingsButton);

    // Style quit button
    {
        auto r = quitButton->getRenderer();
        r->setBackgroundColor(sf::Color(36, 38, 46));
        r->setTextColor(sf::Color(200, 205, 215));
        r->setBackgroundColorHover(sf::Color(85, 42, 48));
        r->setTextColorHover(sf::Color(255, 220, 220));
        r->setBorderColor(sf::Color(58, 60, 72));
        r->setBorders(1);
        r->setRoundedBorderRadius(5 * app.uiScale);
    }
}

void MainMenu::onNewGamePress()
{
    if (!app.hasSkinAssets(app.selectedSkin()))
    {
        if (app.selectedSkin() == Skin::Yoot)
        {
            app.showMessageDialog("Skin Unavailable",
                std::string("The YootTower skin draws its sky, ground and lobby from the YootTower plugins ") +
                yootWorldPluginFile(app.selectedWorld()) + " and " + YootWorldArt::floorPluginFor(app.selectedWorld()) +
                ", which were not found.\n\nAdd the YootTower Plugins folder in Settings or select the OpenSky skin.");
        }
        else if (app.selectedSkin() == Skin::Classic)
        {
            app.showMessageDialog("SIMTOWER.EXE Required",
                "The Classic skin requires SIMTOWER.EXE to load original assets.\n\nPlease configure SIMTOWER.EXE in Settings or select the OpenSky skin.");
        }
        return;
    }
    Game *game = new Game(app);
    game->seedNewTower();
    game->saveFilename = "";
    game->isDirty = false;
    app.popState();
    app.pushState(game);
}
