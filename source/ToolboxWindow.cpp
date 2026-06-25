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
    // Vertical-transport category. Stairs is the grid slot; press-and-hold
    // reveals the Escalator as an overlay child. In SimTower this is the
    // natural primary/secondary pairing: Stairs are the basic transport,
    // Escalator is the upgrade.
    {"stairs",            {"escalator"}},
    {"lobby",             {"floor"}},
    {"elevator-standard", {"elevator-express", "elevator-service"}},
    {"hotel_single",      {"hotel_double", "hotel_suite"}},
    {"condo",             {"yoot_condo"}}
};

// Toolbox layout constants (source pixels; scaled by app->uiScale at render time).
namespace {
    constexpr int kCellW       = 36;   ///< item cell width (rendered; sheet cells are 32)
    constexpr int kCellH       = 36;   ///< item cell height
    constexpr int kPadX        = 5;    ///< horizontal padding around the 3-column grid
    constexpr int kItemsTopY   = 30;   ///< y offset where the item grid begins
    constexpr int kCols        = 3;    ///< grid column count
    constexpr int kHoldMs      = 300;  ///< press-and-hold threshold before the overlay pops
}

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
    window = tgui::ChildWindow::create();
    auto renderer = window->getRenderer();
    renderer->setTitleBarHeight(10 * app->uiScale);
    renderer->setBackgroundColor(sf::Color(30, 30, 35, 200));
    renderer->setTitleBarColor(sf::Color(45, 45, 50));
    renderer->setBorderColor(sf::Color(80, 80, 90));
    renderer->setBorders(1);

    window->setClientSize({118 * app->uiScale, app->uiScale * 252});
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
    // Drop any in-flight overlay state — the widgets are about to be rebuilt.
    holdingParent.clear();
    overlayVisible = false;
    overlayButtons.clear();
    overlayButtonStates.clear();
    overlayToolFor.clear();
    parentSlotIndex.clear();

    // Preserve slot substitutions across reload (e.g. a level-up triggers
    // reload). If a swapped-in child is now locked or missing, drop it; the
    // slot will revert to its parent on the next render pass.
    std::map<std::string, std::string> savedSubs;
    for (auto& kv : parentActiveChild) {
        if (LevelUp::minRatingToBuild(kv.second) <= game->rating)
            savedSubs[kv.first] = kv.second;
    }
    parentActiveChild.clear();

    window->removeAllWidgets();
    buttons.clear();
    toolButtons.clear();
    speedButtons.clear();
    buttonStates.clear();

    // Build the grid: only unlocked parents and standalone tools are shown.
    // Category children (floor, escalator, elevator-express, etc.) never
    // occupy a grid slot — they pop up as an overlay while their parent is
    // held. See CATEGORIES above for the full parent→children mapping.
    std::vector<Item::AbstractPrototype*> visiblePrototypes;
    for (int i = 0; i < game->itemFactory.prototypes.size(); i++)
    {
        Item::AbstractPrototype *prototype = game->itemFactory.prototypes[i];

        if (LevelUp::minRatingToBuild(prototype->id) > game->rating)
            continue;

        std::string parent;
        if (isChildTool(prototype->id, parent))
            continue; // children are overlay-only

        visiblePrototypes.push_back(prototype);
    }

    int numItems = visiblePrototypes.size();
    int totalRows = (numItems + kCols - 1) / kCols;
    int speedY = kItemsTopY + totalRows * kCellH + 8;
    int viewY  = speedY + 24 + 4;  // view-toggle row sits below speed controls
    int clientHeight = viewY + 24 + 3;

    window->setClientSize({(kPadX * 2 + kCellW * kCols) * app->uiScale, clientHeight * app->uiScale});

    // STEP 1: TOOL BUTTONS AT THE TOP (Bulldoze, Finger, Inspect).
    // Source cells are 21x21; render at 24x24 for a chunkier look that
    // matches the enlarged item cells below.
    const int kToolCell = 24;
    auto toolsLayout = tgui::HorizontalLayout::create();
    window->add(toolsLayout);
    toolsLayout->setSize((kToolCell * 3) * app->uiScale, kToolCell * app->uiScale);
    {
        int toolsLayoutW = kToolCell * 3;
        int windowW = kPadX * 2 + kCellW * kCols;
        int toolsX = (windowW - toolsLayoutW) / 2;
        toolsLayout->setPosition(toolsX * app->uiScale, 3 * app->uiScale);
    }

    sf::Texture toolsTexture = app->bitmaps["simtower/ui/toolbox/tools"];

    // Bulldozer
    ButtonState bulldozeState;
    auto bulldozeButton = makeButton(kToolCell, kToolCell, toolsTexture, 0, bulldozeState);
    buttonStates[bulldozeButton] = bulldozeState;
    toolsLayout->add(bulldozeButton);
    toolButtons["bulldozer"] = bulldozeButton;
    bulldozeButton->onPress([this] { game->selectTool("bulldozer"); });

    // Finger (Resize)
    ButtonState fingerState;
    auto fingerButton = makeButton(kToolCell, kToolCell, toolsTexture, 1, fingerState);
    buttonStates[fingerButton] = fingerState;
    toolsLayout->add(fingerButton);
    toolButtons["finger"] = fingerButton;
    fingerButton->onPress([this] { game->selectTool("finger"); });

    // Inspector
    ButtonState inspectState;
    auto inspectButton = makeButton(kToolCell, kToolCell, toolsTexture, 2, inspectState);
    buttonStates[inspectButton] = inspectState;
    toolsLayout->add(inspectButton);
    toolButtons["inspector"] = inspectButton;
    inspectButton->onPress([this] { game->selectTool("inspector"); });

    // STEP 2: BUILDING ITEMS (parents + standalone tools only)
    int row = 0;
    for (int i = 0; i < (int)visiblePrototypes.size(); i++)
    {
        Item::AbstractPrototype *prototype = visiblePrototypes[i];

        char toolname[128];
        snprintf(toolname, 128, "item-%s", prototype->id.c_str());
        std::string toolnameStr(toolname);

        ButtonState state;
        tgui::Button::Ptr button = makeItemButton(prototype, state);

        int xpos = (kPadX + kCellW * (i % kCols)) * app->uiScale;
        int ypos = (kItemsTopY + row * kCellH) * app->uiScale;
        button->setPosition(xpos, ypos);

        window->add(button);
        buttons.insert(button);
        toolButtons[toolnameStr] = button;
        buttonStates[button] = state;

        const std::string protoId = prototype->id;
        if (isCategoryParent(protoId)) {
            // Category parent: press-and-hold reveals its children as an
            // overlay over the neighbouring slots. Selection (parent or a
            // hovered child) is resolved on release in update().
            parentSlotIndex[protoId] = i;
            button->onMousePress([this, protoId](tgui::Vector2f) {
                holdingParent = protoId;
                holdPressTime = std::chrono::steady_clock::now();
                overlayVisible = false;
            });
        } else {
            // Standalone tool: select immediately.
            button->onPress([this, toolnameStr] { onToolButtonPress(toolnameStr.c_str()); });
        }

        if (i % kCols >= kCols - 1)
            row++;
    }

    // STEP 3: SPEED CONTROLS AT THE BOTTOM (horizontally centered)
    const int kSpeedW = 92;
    const int kSpeedH = 24;
    auto speedLayout = tgui::HorizontalLayout::create();
    window->add(speedLayout);
    speedLayout->setSize(kSpeedW * app->uiScale, kSpeedH * app->uiScale);
    {
        int windowW = kPadX * 2 + kCellW * kCols;
        int speedX = (windowW - kSpeedW) / 2;
        speedLayout->setPosition(speedX * app->uiScale, speedY * app->uiScale);
    }

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

    // STEP 4: VIEW TOGGLES at the very bottom (minimap M, finance F, status O).
    // Mirrors the keyboard shortcuts in Game.cpp so the toolbox is
    // self-contained — no need to remember the keybinds.
    const int kViewBtnW = 30;
    const int kViewBtnH = 24;
    const int kViewBtnGap = 4;
    auto makeViewButton = [&](const std::string& label, int offsetX, auto onClick) -> tgui::Button::Ptr {
        auto btn = tgui::Button::create(label);
        btn->setSize(kViewBtnW * app->uiScale, kViewBtnH * app->uiScale);
        btn->setTextSize(10 * app->uiScale);
        btn->getRenderer()->setBackgroundColor(sf::Color(60, 60, 65));
        btn->getRenderer()->setBackgroundColorHover(sf::Color(85, 85, 90));
        btn->getRenderer()->setBackgroundColorDown(sf::Color(40, 40, 45));
        btn->getRenderer()->setTextColor(sf::Color::White);
        btn->getRenderer()->setBorderColor(sf::Color(40, 40, 45));
        btn->getRenderer()->setBorders(1);
        btn->setPosition(offsetX * app->uiScale, viewY * app->uiScale);
        btn->onPress(onClick);
        window->add(btn);
        return btn;
    };
    {
        int windowW = kPadX * 2 + kCellW * kCols;
        int totalW = kViewBtnW * 3 + kViewBtnGap * 2;  // 3 buttons with gaps
        int startX = (windowW - totalW) / 2;
        mapButton = makeViewButton("Map", startX, [this] {
            game->mapWindow.setVisible(!game->mapWindow.isVisible());
        });
        financeButton = makeViewButton("Fin", startX + kViewBtnW + kViewBtnGap, [this] {
            game->financeWindow.setVisible(!game->financeWindow.isVisible());
        });
        statusButton = makeViewButton("View", startX + (kViewBtnW + kViewBtnGap) * 2, [this] {
            game->cycleStatusMode();
        });
    }

    // Restore slot substitutions that survived the rating filter.
    for (auto& kv : savedSubs) {
        parentActiveChild[kv.first] = kv.second;
        rebuildSlotTexture(kv.first, kv.second);
    }

    updateTool();
    updateSpeed();
}

void ToolboxWindow::updateTool()
{
    // Determine which grid button should highlight. If the selected tool is
    // a category child, highlight its parent — the child only exists as a
    // transient overlay, so the parent slot represents it in the grid.
    std::string highlightTool = game->selectedTool;
    std::string selectedProtoId;
    if (game->selectedTool.rfind("item-", 0) == 0) {
        selectedProtoId = game->selectedTool.substr(5);
    }
    std::string parentId;
    if (!selectedProtoId.empty() && isChildTool(selectedProtoId, parentId)) {
        highlightTool = std::string("item-") + parentId;
    }

    for (auto &pair : toolButtons) {
        auto it = buttonStates.find(pair.second);
        if (it != buttonStates.end())
            setButtonVisuals(pair.second, it->second, pair.first == highlightTool);
    }
}

tgui::Button::Ptr ToolboxWindow::makeItemButton(Item::AbstractPrototype* proto, ButtonState& state)
{
    if (proto->id == "yoot_condo")
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
        return makeButton(kCellW, kCellH, yootIconTexture, 0, state);
    }
    sf::Texture itemTexture = app->bitmaps["simtower/ui/toolbox/items"];
    return makeButton(kCellW, kCellH, itemTexture, proto->icon, state, 32, 32);
}

void ToolboxWindow::showOverlayFor(const std::string& parentId)
{
    hideOverlay();

    auto catIt = CATEGORIES.find(parentId);
    if (catIt == CATEGORIES.end()) return;

    int slot = parentSlotIndex.count(parentId) ? parentSlotIndex[parentId] : 0;

    std::string displayedProtoId = parentId;
    auto activeIt = parentActiveChild.find(parentId);
    if (activeIt != parentActiveChild.end() && !activeIt->second.empty())
        displayedProtoId = activeIt->second;

    std::vector<std::string> alternatives;
    alternatives.push_back(parentId);
    alternatives.insert(alternatives.end(), catIt->second.begin(), catIt->second.end());

    int altIdx = 1;
    for (const std::string& protoId : alternatives)
    {
        // The held slot already represents this tool. The overlay shows the
        // other choices in the category, including the displaced parent.
        if (protoId == displayedProtoId)
            continue;

        // Skip tools the player can't build yet.
        if (LevelUp::minRatingToBuild(protoId) > game->rating)
            continue;

        auto pit = game->itemFactory.prototypesById.find(protoId);
        if (pit == game->itemFactory.prototypesById.end())
            continue;
        Item::AbstractPrototype* proto = pit->second;

        ButtonState state;
        tgui::Button::Ptr btn = makeItemButton(proto, state);

        // Place the alternative over the slot immediately following the parent
        // (wrapping within the kCols-wide grid), overlapping whatever
        // parent/standalone tool normally lives there.
        int cslot = slot + altIdx;
        int ccol = cslot % kCols;
        int crow = cslot / kCols;
        int xpos = (kPadX + kCellW * ccol) * app->uiScale;
        int ypos = (kItemsTopY + crow * kCellH) * app->uiScale;
        btn->setPosition(xpos, ypos);

        // Added after the grid widgets, so it renders on top. No signal
        // handlers — selection is resolved by hit-testing in update().
        window->add(btn);

        overlayButtons.push_back(btn);
        overlayButtonStates[btn] = state;
        overlayToolFor[btn] = std::string("item-") + protoId;
        altIdx++;
    }
}

void ToolboxWindow::hideOverlay()
{
    for (tgui::Button::Ptr btn : overlayButtons) {
        window->remove(btn);
    }
    overlayButtons.clear();
    overlayButtonStates.clear();
    overlayToolFor.clear();
}

void ToolboxWindow::rebuildSlotTexture(const std::string& parentId, const std::string& displayedProtoId)
{
    auto btnIt = toolButtons.find("item-" + parentId);
    if (btnIt == toolButtons.end()) return;
    tgui::Button::Ptr btn = btnIt->second;

    auto protoIt = game->itemFactory.prototypesById.find(displayedProtoId);
    if (protoIt == game->itemFactory.prototypesById.end()) return;
    Item::AbstractPrototype* proto = protoIt->second;

    // Build a fresh ButtonState from the displayed prototype's icon.
    ButtonState newState;
    tgui::Button::Ptr tmp = makeItemButton(proto, newState);

    // Swap the textures onto the existing button widget.
    buttonStates[btn] = newState;

    // Determine the highlighted state by matching the displayed tool name
    // against the currently selected one (which may be a child, in which
    // case updateTool() maps it back to the parent for highlighting).
    std::string displayedTool = "item-" + displayedProtoId;
    bool checked = (game->selectedTool == displayedTool);
    setButtonVisuals(btn, newState, checked);
}

void ToolboxWindow::update()
{
    if (holdingParent.empty())
        return;

    const bool mouseDown = sf::Mouse::isButtonPressed(sf::Mouse::Button::Left);

    // Reveal the overlay once the hold threshold has elapsed.
    if (!overlayVisible) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - holdPressTime).count();
        if (elapsed >= kHoldMs) {
            showOverlayFor(holdingParent);
            overlayVisible = true;
        }
    }

    // Refresh the hover highlight on whichever child the cursor is over.
    if (overlayVisible) {
        sf::Vector2i mpi = sf::Mouse::getPosition(app->window);
        sf::Vector2f mp(static_cast<float>(mpi.x), static_cast<float>(mpi.y));
        for (tgui::Button::Ptr btn : overlayButtons) {
            sf::Vector2f pos = btn->getAbsolutePosition();
            sf::Vector2f size = btn->getSize();
            const bool over = (mp.x >= pos.x && mp.x < pos.x + size.x &&
                               mp.y >= pos.y && mp.y < pos.y + size.y);
            auto it = overlayButtonStates.find(btn);
            if (it != overlayButtonStates.end())
                setButtonVisuals(btn, it->second, over);
        }
    }

    // Resolve the press when the mouse button is released. We poll the
    // physical button state rather than relying on TGUI's onMouseRelease
    // (which is unreliable once overlay widgets are stacked on top of the
    // pressed parent).
    if (!mouseDown) {
        // Decide what was selected:
        //  - Short click: select the parent tool. This keeps the visible parent
        //    slot from staying highlighted while a child such as Floor remains
        //    selected, which made Lobby placement look broken.
        //  - Press+hold then release on an overlay alternative: select it and
        //    swap the slot to display it.
        //  - Press+hold then release off any overlay alternative: select parent.
        std::string selectedProto;
        if (overlayVisible) {
            sf::Vector2i mpi = sf::Mouse::getPosition(app->window);
            sf::Vector2f mp(static_cast<float>(mpi.x), static_cast<float>(mpi.y));
            for (tgui::Button::Ptr btn : overlayButtons) {
                sf::Vector2f pos = btn->getAbsolutePosition();
                sf::Vector2f size = btn->getSize();
                if (mp.x >= pos.x && mp.x < pos.x + size.x &&
                    mp.y >= pos.y && mp.y < pos.y + size.y) {
                    auto tit = overlayToolFor.find(btn);
                    if (tit != overlayToolFor.end()) {
                        // Strip the "item-" prefix to get the proto id.
                        const std::string& toolname = tit->second;
                        selectedProto = (toolname.rfind("item-", 0) == 0)
                                          ? toolname.substr(5) : toolname;
                        break;
                    }
                }
            }
        }
        if (selectedProto.empty()) {
            selectedProto = holdingParent;
        }

        // Swap the slot's texture if the displayed tool changed. When the
        // parent is selected again, remove the substitution entirely.
        auto displayedIt = parentActiveChild.find(holdingParent);
        std::string displayed = (displayedIt != parentActiveChild.end()) ? displayedIt->second : holdingParent;
        if (displayed != selectedProto) {
            if (selectedProto == holdingParent)
                parentActiveChild.erase(holdingParent);
            else
                parentActiveChild[holdingParent] = selectedProto;
            rebuildSlotTexture(holdingParent, selectedProto);
        }

        hideOverlay();
        overlayVisible = false;
        holdingParent.clear();

        std::string toolname = std::string("item-") + selectedProto;
        onToolButtonPress(toolname.c_str());
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

tgui::Button::Ptr ToolboxWindow::makeButton(int width, int height, sf::Texture textureMap, int index, ButtonState &state, int cellW, int cellH)
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
    // Default logic (e.g. building items or fallbacks). The source texture
    // is sliced into cells of size (cellW x cellH); if the caller did not
    // override them they default to the target render size. This lets us
    // slice a 32px-cell sheet but render the button at e.g. 36px.
    else {
        int cw = cellW > 0 ? cellW : width;
        int ch = cellH > 0 ? cellH : height;
        normalRect = sf::IntRect({index * cw, 0}, {cw, ch});
        bool hasCheckedRow = (texHeight >= static_cast<unsigned>(ch * 2));
        if (hasCheckedRow) {
            checkedRect = sf::IntRect({index * cw, ch}, {cw, ch});
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
