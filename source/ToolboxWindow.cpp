/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "ToolboxWindow.h"
#include "GameObject.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Renderers/ButtonRenderer.hpp>
#include <TGUI/TwoFingerScrollDetect.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/HorizontalLayout.hpp>
#include <TGUI/Widgets/VerticalLayout.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <cstdint>
#include <algorithm>
#include <fstream>
#include "Application.h"
#include "Game.h"
#include "Item/YootCondo.h"
#include "LevelUp.h"

using namespace OT;

static const std::map<std::string, std::vector<std::string>> CATEGORIES = {
    {"lobby", {"floor", "stairs"}},
    {"standard_elevator", {"express_elevator", "service_elevator"}},
    {"hotel_single", {"hotel_double", "hotel_suite"}},
    {"condo", {"yoot_condo"}}
};

bool ToolboxWindow::isChildTool(const std::string& toolId, std::string& outParentId) const {
    for (auto const& pair : CATEGORIES) {
        for (auto const& child : pair.second) {
            if (child == toolId) {
                outParentId = pair.first;
                return true;
            }
        }
    }
    return false;
}

bool ToolboxWindow::isCategoryParent(const std::string& toolId) const {
    return CATEGORIES.find(toolId) != CATEGORIES.end();
}

ToolboxWindow::ToolboxWindow(Game *game) : GameObject(game)
{
    expandedCategories["lobby"] = false;
    expandedCategories["standard_elevator"] = false;
    expandedCategories["hotel_single"] = false;
    expandedCategories["condo"] = false;

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

static bool loadGeneratedSpeedTexture(sf::Texture &texture)
{
    sf::Image image({92, 48}, sf::Color::Transparent);
    const sf::Color face[2] = {sf::Color(214, 214, 214), sf::Color(68, 68, 68)};
    const sf::Color light[2] = {sf::Color::White, sf::Color(112, 112, 112)};
    const sf::Color shadow[2] = {sf::Color(84, 84, 84), sf::Color(18, 18, 18)};
    const sf::Color glyph[2] = {sf::Color(20, 20, 20), sf::Color::White};

    auto fillRect = [&](unsigned int x, unsigned int y, unsigned int w, unsigned int h, sf::Color color) {
        for (unsigned int py = y; py < y + h; py++)
            for (unsigned int px = x; px < x + w; px++)
                image.setPixel({px, py}, color);
    };
    auto fillTriangle = [&](int ax, int ay, int bx, int by, int cx, int cy, sf::Color color) {
        const int minX = std::min(ax, std::min(bx, cx));
        const int maxX = std::max(ax, std::max(bx, cx));
        const int minY = std::min(ay, std::min(by, cy));
        const int maxY = std::max(ay, std::max(by, cy));
        const int area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
        for (int y = minY; y <= maxY; y++)
        {
            for (int x = minX; x <= maxX; x++)
            {
                const int w0 = (bx - ax) * (y - ay) - (by - ay) * (x - ax);
                const int w1 = (cx - bx) * (y - by) - (cy - by) * (x - bx);
                const int w2 = (ax - cx) * (y - cy) - (ay - cy) * (x - cx);
                if ((area >= 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                    (area < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0))
                    image.setPixel({static_cast<unsigned int>(x), static_cast<unsigned int>(y)}, color);
            }
        }
    };
    auto drawPlay = [&](unsigned int ox, unsigned int oy, unsigned int count, sf::Color color) {
        for (unsigned int i = 0; i < count; i++)
        {
            const int x = static_cast<int>(ox + 5 + i * 5);
            const int y = static_cast<int>(oy + 5);
            fillTriangle(x, y, x, y + 14, x + 8, y + 7, color);
        }
    };

    for (unsigned int row = 0; row < 2; row++)
    {
        for (unsigned int index = 0; index < 4; index++)
        {
            const unsigned int x = index * 23;
            const unsigned int y = row * 24;
            fillRect(x, y, 23, 24, face[row]);
            fillRect(x, y, 23, 1, light[row]);
            fillRect(x, y, 1, 24, light[row]);
            fillRect(x, y + 23, 23, 1, shadow[row]);
            fillRect(x + 22, y, 1, 24, shadow[row]);

            if (index == 0)
            {
                fillRect(x + 6, y + 5, 4, 14, glyph[row]);
                fillRect(x + 13, y + 5, 4, 14, glyph[row]);
            }
            else
            {
                drawPlay(x, y, index, glyph[row]);
            }
        }
    }

    return texture.loadFromImage(image);
}

void ToolboxWindow::reload()
{
    window->removeAllWidgets();
    buttons.clear();
    toolButtons.clear();
    speedButtons.clear();
    buttonStates.clear();

    // Auto-expand category of selected tool if it is a child
    std::string selectedProtoId = "";
    if (game->selectedTool.rfind("item-", 0) == 0) {
        selectedProtoId = game->selectedTool.substr(5);
    }
    std::string parentId;
    if (!selectedProtoId.empty() && isChildTool(selectedProtoId, parentId)) {
        expandedCategories[parentId] = true;
    }

    // Filter prototypes to only include visible ones
    std::vector<Item::AbstractPrototype*> visiblePrototypes;
    for (int i = 0; i < game->itemFactory.prototypes.size(); i++)
    {
        Item::AbstractPrototype *prototype = game->itemFactory.prototypes[i];
        
        std::string parent;
        if (isChildTool(prototype->id, parent)) {
            if (expandedCategories[parent]) {
                visiblePrototypes.push_back(prototype);
            }
        } else {
            visiblePrototypes.push_back(prototype);
        }
    }

    // Calculate total rows and sizes dynamically to prevent overlapping when new items are registered
    int numItems = visiblePrototypes.size();
    int totalRows = (numItems + 2) / 3;
    int speedY = 25 + totalRows * 32 + 8;
    int clientHeight = speedY + 24 + 3;

    window->setClientSize({106 * app->uiScale, clientHeight * app->uiScale});

    // STEP 1: MOVE TOOL BUTTONS TO THE TOP (Bulldoze, Finger, Inspect)
    auto toolsLayout = tgui::HorizontalLayout::create();
    window->add(toolsLayout);
    toolsLayout->setSize(63 * app->uiScale, 21 * app->uiScale);
    toolsLayout->setPosition(21 * app->uiScale, 2 * app->uiScale);

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

    // STEP 2: BUILDING ITEMS
    int row = 0;
    sf::Texture itemTexture = app->bitmaps["simtower/ui/toolbox/items"];
    for (int i = 0; i < visiblePrototypes.size(); i++)
    {
        Item::AbstractPrototype *prototype = visiblePrototypes[i];

        char toolname[128];
        snprintf(toolname, 128, "item-%s", prototype->id.c_str());

        ButtonState state;
        tgui::Button::Ptr button;
        if (prototype->id == "yoot_condo")
        {
            sf::Texture yootIconTexture;
            std::string pathStr;
            DataManager::Paths possiblePaths = app->data.paths("Plugins/Condo.t2p");
            for (const auto &p : possiblePaths) {
                std::ifstream f(p.c_str());
                if (f.good()) {
                    pathStr = p.str();
                    break;
                }
            }
            if (!pathStr.empty()) {
                Item::YootCondo::loadPEBitmap(pathStr, 100, yootIconTexture, sf::Color(255, 255, 255));
            }
            button = makeButton(26, 26, yootIconTexture, 0, state);
        }
        else
        {
            button = makeButton(32, 32, itemTexture, prototype->icon, state);
        }
        int xpos = (5 + 32 * (i % 3)) * app->uiScale;
        int ypos = (25 + row * 32) * app->uiScale;

        button->setPosition(xpos, ypos);

        // Star-rating gate: disable buttons for prototypes the player can't
        // build yet. Attach a tooltip explaining the unlock requirement.
        int minRating = LevelUp::minRatingToBuild(prototype->id);
        if (minRating > game->rating)
        {
            button->setEnabled(false);
            auto tip = tgui::Label::create();
            tip->setText(prototype->name + " unlocks at " +
                         std::to_string(minRating + 1) + " stars");
            tip->setTextSize(12);
            button->setToolTip(tip);
        }

        window->add(button);
        buttons.insert(button);
        toolButtons[toolname] = button;
        buttonStates[button] = state;

        std::string toolnameStr(toolname);
        
        button->onMousePress([this, toolnameStr](tgui::Vector2f pos) {
            pressedTool = toolnameStr;
            pressTime = std::chrono::steady_clock::now();
        });

        button->onMouseRelease([this, toolnameStr](tgui::Vector2f pos) {
            if (pressedTool == toolnameStr) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - pressTime
                ).count();

                std::string protoId = (toolnameStr.rfind("item-", 0) == 0) ? toolnameStr.substr(5) : "";
                if (elapsed >= 300 && !protoId.empty() && isCategoryParent(protoId)) {
                    expandedCategories[protoId] = !expandedCategories[protoId];
                    reload();
                } else {
                    onToolButtonPress(toolnameStr.c_str());
                }
            }
        });

        if (i % 3 >= 2)
            row++;
    }

    // STEP 3: MOVE SPEED CONTROLS TO THE BOTTOM
    auto speedLayout = tgui::HorizontalLayout::create();
    window->add(speedLayout);
    speedLayout->setSize(92 * app->uiScale, 24 * app->uiScale);
    speedLayout->setPosition(7 * app->uiScale, speedY * app->uiScale);

    sf::Texture speedTexture = app->bitmaps["simtower/ui/toolbox/speed"];
    if (speedTexture.getSize() == sf::Vector2u(256, 32))
        loadGeneratedSpeedTexture(speedTexture);
    for (int i = 0; i < 4; i++)
    {
        ButtonState state;
        auto button = makeButton(23, 24, speedTexture, i, state);
        speedLayout->add(button);
        speedButtons[i] = button;
        buttonStates[button] = state;
        button->onPress([this, i] { onSpeedButtonPress(i); });
    }

    updateTool();
    updateSpeed();
}

void ToolboxWindow::updateTool()
{
    // Auto-expand category of selected tool if it is a child
    std::string selectedProtoId = "";
    if (game->selectedTool.rfind("item-", 0) == 0) {
        selectedProtoId = game->selectedTool.substr(5);
    }
    std::string parentId;
    if (!selectedProtoId.empty() && isChildTool(selectedProtoId, parentId)) {
        if (!expandedCategories[parentId]) {
            expandedCategories[parentId] = true;
            reload();
            return;
        }
    }

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
