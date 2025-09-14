#include <cassert>
#include <cmath>
#include <TGUI/TGUI.hpp>
#include <TGUI/Widgets/VerticalLayout.hpp>
#include <TGUI/Widgets/HorizontalLayout.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include "Game.h"
#include "ToolboxWindow.h"
#include <chrono>
#include <algorithm>
#include <set>
#include <vector>
#include <string>

using namespace OT;

// Define a constant for the long-press threshold
const double LONG_PRESS_THRESHOLD = 0.3; // seconds

ToolboxWindow::ToolboxWindow(Game * game) : GameObject(game) {
    window = nullptr;
    float uiScale = game->app.guiManager.getBackend()->getUIScale();
    window = tgui::ChildWindow::create();
    window->getRenderer()->setTitleBarHeight(10 * uiScale);
    window->setClientSize(tgui::Layout2d(106 * uiScale, 220 * uiScale));
    window->setPosition(0, 24 * uiScale);
    window->getRenderer()->setBackgroundColor(tgui::Color::White);
    game->app.guiManager.getBackend()->getGui()->add(window);
    reload();
}

void ToolboxWindow::advance(double dt)
{
	// TODO: Implement TGUI long-press handling
}

void ToolboxWindow::close()
{
	buttons.clear();

	if (window) {
		game->app.guiManager.getBackend()->getGui()->remove(window);
	}
	window = nullptr;
}

void ToolboxWindow::reload()
{
    window->removeAllWidgets();
    buttons.clear();

    float uiScale = game->app.guiManager.getBackend()->getUIScale();

    auto topLayout = tgui::VerticalLayout::create();
    window->add(topLayout);
    topLayout->setSize(tgui::Layout2d("100%", "100%"));

    // tool buttons
    auto toolsLayout = tgui::HorizontalLayout::create();
    topLayout->add(toolsLayout, "toolsLayout");
    toolsLayout->setSize(tgui::Layout2d("100%", 21 * uiScale));

    toolsLayout->addSpace(0.6f);
    auto bulldozeButton = makeButton(21, game->app.bitmaps["simtower/ui/toolbox/tools"], 0);
    toolsLayout->add(bulldozeButton);
    bulldozeButton->onPress([this]{ game->selectTool("bulldozer"); });
    buttons.insert(bulldozeButton);

    auto fingerButton = makeButton(21, game->app.bitmaps["simtower/ui/toolbox/tools"], 1);
    toolsLayout->add(fingerButton);
    fingerButton->onPress([this]{ game->selectTool("finger"); });
    buttons.insert(fingerButton);

    auto inspectButton = makeButton(21, game->app.bitmaps["simtower/ui/toolbox/tools"], 2);
    toolsLayout->add(inspectButton);
    inspectButton->onPress([this]{game->selectTool("inspector");});
    buttons.insert(inspectButton);
    toolsLayout->addSpace(0.6f);

    // item buttons
    // Group prototypes by category and sort by toolboxOrder
    std::map<int, std::vector<const OT::Item::AbstractPrototype*>> prototypesByCategory;
    for (const auto& prototype : game->itemFactory.prototypes) {
        prototypesByCategory[prototype->toolboxCategory].push_back(prototype);
    }
    // Sort each category by toolboxOrder
    for (auto& pair : prototypesByCategory) {
        std::sort(pair.second.begin(), pair.second.end(), [](const OT::Item::AbstractPrototype* a, const OT::Item::AbstractPrototype* b) {
            return a->toolboxOrder < b->toolboxOrder;
        });
    }
    // Create layouts for each category
    for (const auto& pair : prototypesByCategory) {
        int cat = pair.first;
        const auto& protos = pair.second;
        // Create a vertical layout for this category
        auto catLayout = tgui::VerticalLayout::create();
        topLayout->add(catLayout);
        // Now, add buttons in rows of 3
        size_t numButtons = protos.size();
        size_t buttonsPerRow = 3;
        size_t numRows = (numButtons + buttonsPerRow - 1) / buttonsPerRow;
        for (size_t row = 0; row < numRows; ++row) {
            auto rowLayout = tgui::HorizontalLayout::create();
            catLayout->add(rowLayout);
            for (size_t col = 0; col < buttonsPerRow; ++col) {
                size_t index = row * buttonsPerRow + col;
                if (index >= numButtons) break;
                const auto* prototype = protos[index];
                std::string toolname = "item-" + prototype->id;
                auto button = makeButton(32, game->app.bitmaps["simtower/ui/toolbox/items"], prototype->icon);
                button->onPress([this, toolname](){ selectItem(toolname); });
                rowLayout->add(button);
                buttons.insert(button);
            }
        }
    }

    updateSpeed();
    updateTool();
    updateAvailability();
}

// ProcessEvent method removed - not needed for TGUI

void ToolboxWindow::handleMouseDown(tgui::Widget::Ptr element) {
	// TODO: Implement TGUI mouse down handling
}

void ToolboxWindow::handleMouseUp(tgui::Widget::Ptr element) {
	// TODO: Implement TGUI mouse up handling
}

void ToolboxWindow::handleMouseMove() {
	// TODO: Implement TGUI mouse move handling
}

void ToolboxWindow::handleClick(tgui::Widget::Ptr element) {
	// TODO: Implement TGUI click handling
}

void ToolboxWindow::startLongPressTimer(tgui::Widget::Ptr element) {
	// TODO: Implement TGUI long press timer
}

void ToolboxWindow::cancelLongPressTimer() {
	// TODO: Implement TGUI long press timer cancellation
}

bool ToolboxWindow::isContextMenuOpen() {
	// TODO: Implement TGUI context menu check
	return false;
}

void ToolboxWindow::dismissContextMenu() {
	// TODO: Implement TGUI context menu dismissal
}

void ToolboxWindow::handleContextMenuClick(tgui::Widget::Ptr element) {
	// TODO: Implement TGUI context menu click handling
}

void ToolboxWindow::selectItem(const std::string& id) {
	// Pass the full button ID (with "item-" prefix) to selectTool as expected
	game->selectTool(id.c_str());
}

void ToolboxWindow::restoreOriginalLayout() {
	// TODO: Implement TGUI layout restoration
}

void ToolboxWindow::triggerLongPress(tgui::Widget::Ptr element) {
	// TODO: Implement TGUI long press triggering
}

void ToolboxWindow::updateSpeed()
{
	// TODO: Implement TGUI speed button updates
}

void ToolboxWindow::updateTool()
{
	// TODO: Implement TGUI tool selection updates
}

void ToolboxWindow::updateAvailability()
{
	// TODO: Implement TGUI availability updates
}

tgui::BitmapButton::Ptr ToolboxWindow::makeButton(int size, const sf::Texture& textureMap, int index) {
    float uiScale = game->app.guiManager.getBackend()->getUIScale();
    int scaledSize = size * uiScale;

    // Extract the sub-rectangle from the spritesheet
    sf::IntRect rect(index * size, 0, size, size);
    sf::Image iconImage = textureMap.copyToImage();
    sf::Image subImage;
    subImage.create(size, size);
    subImage.copy(iconImage, 0, 0, rect, false);

    // Scale the image to the button size
    sf::Image scaledImage;
    scaledImage.create(scaledSize, scaledSize);
    // Simple nearest-neighbor scaling (for pixel art look)
    for (int y = 0; y < scaledSize; ++y) {
        for (int x = 0; x < scaledSize; ++x) {
            int srcX = x * size / scaledSize;
            int srcY = y * size / scaledSize;
            scaledImage.setPixel(x, y, subImage.getPixel(srcX, srcY));
        }
    }

    // Create a new SFML texture from the scaled image
    sf::Texture scaledTexture;
    scaledTexture.loadFromImage(scaledImage);

    // Load into TGUI texture
    tgui::Texture tguiImage;
tguiImage.loadFromPixelData(scaledTexture.getSize(), scaledTexture.copyToImage().getPixelsPtr());

    tgui::BitmapButton::Ptr b = tgui::BitmapButton::create();
    b->setSize(scaledSize, scaledSize);
    b->setImage(tguiImage);
    return b;
}