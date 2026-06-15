/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "ToolboxWindow.h"
#include "GameObject.h"
#include <SFML/Graphics/Texture.hpp>
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Renderers/ButtonRenderer.hpp>
#include <TGUI/TwoFingerScrollDetect.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/HorizontalLayout.hpp>
#include <TGUI/Widgets/VerticalLayout.hpp>
#include <cstdint>
#include <algorithm>
#include "Application.h"
#include "Game.h"

using namespace OT;

ToolboxWindow::ToolboxWindow(Game *game) : GameObject(game)
{
    window = tgui::ChildWindow::create();
    auto renderer = window->getRenderer();
    renderer->setTitleBarHeight(10 * app->uiScale);
    renderer->setBackgroundColor(sf::Color(30, 30, 35, 200));
    renderer->setTitleBarColor(sf::Color(45, 45, 50));
    renderer->setBorderColor(sf::Color(80, 80, 90));
    renderer->setBorders(1);

    window->setClientSize({106 * app->uiScale, app->uiScale * 220});
    window->setPosition(0, 24 * app->uiScale);

    reload();
    app->gui.add(window);
}

void ToolboxWindow::close()
{
    if (window) {
        window->removeAllWidgets();
        app->gui.remove(window);
    }
}

static void setButtonVisuals(tgui::Button::Ptr button, const ToolboxWindow::ButtonState &state, bool checked)
{
    auto renderer = button->getRenderer();
    if (checked) {
        renderer->setTexture(state.checked);
        renderer->setTextureHover(state.checked);
        renderer->setTextureDown(state.checked);
    } else {
        renderer->setTexture(state.normal);
        renderer->setTextureHover(state.hover);
        renderer->setTextureDown(state.checked);
    }
}

void ToolboxWindow::reload()
{
    window->removeAllWidgets();
    buttons.clear();
    toolButtons.clear();
    speedButtons.clear();
    buttonStates.clear();

    auto topLayout = tgui::VerticalLayout::create();
    window->add(topLayout);
    topLayout->setSize("100%", "100%");

    // STEP 1: MOVE TOOL BUTTONS TO THE TOP (Bulldoze, Finger, Inspect)
    auto toolsLayout = tgui::HorizontalLayout::create();
    topLayout->add(toolsLayout, "toolsLayout");
    toolsLayout->setSize("100%", 21 * app->uiScale);
    toolsLayout->addSpace(0.6f);

    sf::Texture toolsTexture = app->bitmaps["simtower/ui/toolbox/tools"];

    // Bulldozer
    ButtonState bulldozeState;
    auto bulldozeButton = makeButton(21, 21, toolsTexture, 0, bulldozeState);
    buttonStates[bulldozeButton] = bulldozeState;
    toolsLayout->add(bulldozeButton);
    toolButtons["bulldozer"] = bulldozeButton;
    bulldozeButton->onPress([this] { game->selectTool("bulldozer"); });

    // Finger (Resize)
    ButtonState fingerState;
    auto fingerButton = makeButton(21, 21, toolsTexture, 1, fingerState);
    buttonStates[fingerButton] = fingerState;
    toolsLayout->add(fingerButton);
    toolButtons["finger"] = fingerButton;
    fingerButton->onPress([this] { game->selectTool("finger"); });

    // Inspector
    ButtonState inspectState;
    auto inspectButton = makeButton(21, 21, toolsTexture, 2, inspectState);
    buttonStates[inspectButton] = inspectState;
    toolsLayout->add(inspectButton);
    toolButtons["inspector"] = inspectButton;
    inspectButton->onPress([this] { game->selectTool("inspector"); });
    toolsLayout->addSpace(0.6f);

    // STEP 2: BUILDING ITEMS
    int row = 0;
    sf::Texture itemTexture = app->bitmaps["simtower/ui/toolbox/items"];
    for (int i = 0; i < game->itemFactory.prototypes.size(); i++)
    {
        Item::AbstractPrototype *prototype = game->itemFactory.prototypes[i];

        char toolname[128];
        snprintf(toolname, 128, "item-%s", prototype->id.c_str());

        ButtonState state;
        auto button = makeButton(32, 32, itemTexture, prototype->icon, state);
        int xpos = (5 + 32 * (i % 3)) * app->uiScale;
        int ypos = (25 + row * 32) * app->uiScale;

        button->setPosition(xpos, ypos);
        window->add(button);
        buttons.insert(button);
        toolButtons[toolname] = button;
        buttonStates[button] = state;
        button->onPress(&ToolboxWindow::onToolButtonPress, this, toolname);
        if (i % 3 >= 2)
            row++;
    }

    // STEP 3: MOVE SPEED CONTROLS TO THE BOTTOM
    auto speedLayout = tgui::HorizontalLayout::create();
    topLayout->add(speedLayout, "speedLayout");
    speedLayout->setSize("100%", 24 * app->uiScale);
    speedLayout->addSpace(0.6f);

    sf::Texture speedTexture = app->bitmaps["simtower/ui/toolbox/speed"];
    for (int i = 0; i < 4; i++)
    {
        ButtonState state;
        auto button = makeButton(23, 24, speedTexture, i, state);
        speedLayout->add(button);
        speedButtons[i] = button;
        buttonStates[button] = state;
        button->onPress([this, i] { onSpeedButtonPress(i); });
    }
    speedLayout->addSpace(0.6f);

    updateTool();
    updateSpeed();
}

void ToolboxWindow::updateTool()
{
    for (auto &pair : toolButtons) {
        auto it = buttonStates.find(pair.second);
        if (it != buttonStates.end())
            setButtonVisuals(pair.second, it->second, pair.first == game->selectedTool);
    }
}

void ToolboxWindow::updateSpeed()
{
    for (auto &pair : speedButtons) {
        auto it = buttonStates.find(pair.second);
        if (it != buttonStates.end())
            setButtonVisuals(pair.second, it->second, pair.first == game->speedMode);
    }
}

void ToolboxWindow::onToolButtonPress(const char *tool)
{
    LOG(IMPORTANT, "tool button pressed: %s", tool);
    game->selectTool(tool);
}

void ToolboxWindow::onSpeedButtonPress(int speedMode)
{
    LOG(IMPORTANT, "speed button pressed: %i", speedMode);
    if (speedMode == 0) {
        // Pause/Play toggle: restore previous speed, or default to 1x
        if (game->speedMode == 0)
            game->setSpeedMode(lastSpeedMode > 0 ? lastSpeedMode : 1);
        else {
            lastSpeedMode = game->speedMode;
            game->setSpeedMode(0);
        }
    } else {
        game->setSpeedMode(speedMode);
    }
}

tgui::Button::Ptr ToolboxWindow::makeButton(int width, int height, sf::Texture textureMap, int index, ButtonState &state)
{
    int scaledWidth = width * app->uiScale;
    int scaledHeight = height * app->uiScale;

    sf::Image iconImage = textureMap.copyToImage();
    unsigned int texWidth = iconImage.getSize().x;
    unsigned int texHeight = iconImage.getSize().y;

    sf::IntRect normalRect;
    sf::IntRect checkedRect;
    sf::IntRect hoverRect;

    bool customChecked = false;
    bool customHover = false;

    // Detect tools texture
    if (texHeight == 21 && (texWidth == 192 || texWidth == 64)) {
        if (texWidth == 192) {
            // Real SimTower tools texture (merged from 3 resources of 64x21)
            normalRect = sf::IntRect({index * 21, 0}, {21, 21});
            checkedRect = sf::IntRect({64 + index * 21, 0}, {21, 21});
            hoverRect = sf::IntRect({128 + index * 21, 0}, {21, 21});
            customChecked = true;
            customHover = true;
        } else {
            // Fallback tools texture (64x21)
            normalRect = sf::IntRect({index * 21, 0}, {21, 21});
        }
    }
    // Detect speed texture
    else if (texWidth == 256 && texHeight == 32) {
        // Real SimTower speed texture (merged from 4 resources of 64x32)
        // Each button is 23x24 located at X = 20, Y = 4 relative to resource start
        normalRect = sf::IntRect({index * 64 + 20, 4}, {23, 24});
    }
    // Default logic (e.g. building items or fallbacks)
    else {
        normalRect = sf::IntRect({index * width, 0}, {width, height});
        bool hasCheckedRow = (texHeight >= static_cast<unsigned>(height * 2));
        if (hasCheckedRow) {
            checkedRect = sf::IntRect({index * width, height}, {width, height});
            customChecked = true;
        }
    }

    auto extract = [&](const sf::IntRect &rect) {
        sf::Image subImage;
        subImage.resize({static_cast<unsigned>(rect.size.x), static_cast<unsigned>(rect.size.y)});
        [[maybe_unused]] bool copied = subImage.copy(iconImage, {0, 0}, rect, false);
        return subImage;
    };

    // Extract normal image using the calculated normalRect width/height
    int actualWidth = normalRect.size.x;
    int actualHeight = normalRect.size.y;
    sf::Image normalImage = extract(normalRect);

    sf::Image checkedImage;
    if (customChecked) {
        checkedImage = extract(checkedRect);
    } else {
        checkedImage.resize({static_cast<unsigned>(actualWidth), static_cast<unsigned>(actualHeight)});
        for (unsigned y = 0; y < static_cast<unsigned>(actualHeight); y++) {
            for (unsigned x = 0; x < static_cast<unsigned>(actualWidth); x++) {
                sf::Color c = normalImage.getPixel({x, y});
                c.r = static_cast<std::uint8_t>(c.r * 0.7f);
                c.g = static_cast<std::uint8_t>(c.g * 0.7f);
                c.b = static_cast<std::uint8_t>(c.b * 0.7f);
                checkedImage.setPixel({x, y}, c);
            }
        }
    }

    sf::Image hoverImage;
    if (customHover) {
        hoverImage = extract(hoverRect);
    } else {
        hoverImage.resize({static_cast<unsigned>(actualWidth), static_cast<unsigned>(actualHeight)});
        for (unsigned y = 0; y < static_cast<unsigned>(actualHeight); y++) {
            for (unsigned x = 0; x < static_cast<unsigned>(actualWidth); x++) {
                sf::Color c = normalImage.getPixel({x, y});
                c.r = static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.r * 1.15f + 20)));
                c.g = static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.g * 1.15f + 20)));
                c.b = static_cast<std::uint8_t>(std::min(255, static_cast<int>(c.b * 1.15f + 20)));
                hoverImage.setPixel({x, y}, c);
            }
        }
    }

    // Helper to scale an image to the button size using nearest-neighbor
    auto scaleImage = [&](const sf::Image &src) {
        sf::Image scaled;
        scaled.resize({static_cast<unsigned>(scaledWidth), static_cast<unsigned>(scaledHeight)});
        for (int y = 0; y < scaledHeight; ++y) {
            for (int x = 0; x < scaledWidth; ++x) {
                int srcX = x * actualWidth / scaledWidth;
                int srcY = y * actualHeight / scaledHeight;
                scaled.setPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)},
                                src.getPixel({static_cast<unsigned>(srcX), static_cast<unsigned>(srcY)}));
            }
        }
        return scaled;
    };

    sf::Image scaledNormal = scaleImage(normalImage);
    sf::Image scaledHover = scaleImage(hoverImage);
    sf::Image scaledChecked = scaleImage(checkedImage);

    // Load into TGUI textures
    state.normal.loadFromPixelData({static_cast<unsigned>(scaledWidth), static_cast<unsigned>(scaledHeight)}, scaledNormal.getPixelsPtr());
    state.hover.loadFromPixelData({static_cast<unsigned>(scaledWidth), static_cast<unsigned>(scaledHeight)}, scaledHover.getPixelsPtr());
    state.checked.loadFromPixelData({static_cast<unsigned>(scaledWidth), static_cast<unsigned>(scaledHeight)}, scaledChecked.getPixelsPtr());

    tgui::Button::Ptr b = tgui::Button::create();
    b->setSize(scaledWidth, scaledHeight);
    auto renderer = b->getRenderer();
    renderer->setBorders({0, 0, 0, 0});
    renderer->setTexture(state.normal);
    renderer->setTextureHover(state.hover);
    renderer->setTextureDown(state.checked);
    return b;
}
