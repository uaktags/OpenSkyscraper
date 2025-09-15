#include <cassert>
#include <cmath>
#include <TGUI/TGUI.hpp>
#include <TGUI/Widgets/VerticalLayout.hpp>
#include <TGUI/Widgets/HorizontalLayout.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include <TGUI/Widgets/ScrollablePanel.hpp>
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
    window = tgui::ChildWindow::create();
    window->getRenderer()->setTitleBarHeight(15); // 10*1.5
    window->setClientSize(tgui::Layout2d(180, 330)); // 120*1.5, 220*1.5
    window->setPosition(0, 33); // 0, 22*1.5
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

    auto scrollablePanel = tgui::ScrollablePanel::create();
    window->add(scrollablePanel);
    scrollablePanel->setSize(tgui::Layout2d("100%", "100%"));

    auto verticalLayout = tgui::VerticalLayout::create();
    scrollablePanel->add(verticalLayout);
    verticalLayout->setSize("100%", "100%");

    // Speed controls
    auto speedLayout = tgui::HorizontalLayout::create();
    verticalLayout->add(speedLayout);
    speedLayout->setSize("100%", 32);
    speedLayout->addSpace(0.5f);
    auto pauseButton = makeButton(21, game->app.bitmaps["simtower/ui/speed"], 0);
    speedLayout->add(pauseButton);
    pauseButton->onPress([this]{ game->setSpeedMode(0); });
    auto playButton = makeButton(21, game->app.bitmaps["simtower/ui/speed"], 1);
    speedLayout->add(playButton);
    playButton->onPress([this]{ game->setSpeedMode(1); });
    auto twoXButton = makeButton(21, game->app.bitmaps["simtower/ui/speed"], 2);
    speedLayout->add(twoXButton);
    twoXButton->onPress([this]{ game->setSpeedMode(2); });
    auto fastForwardButton = makeButton(21, game->app.bitmaps["simtower/ui/speed"], 3);
    speedLayout->add(fastForwardButton);
    fastForwardButton->onPress([this]{ game->setSpeedMode(3); });
    speedLayout->addSpace(0.5f);

    // tool buttons
    auto toolsLayout = tgui::HorizontalLayout::create();
    verticalLayout->add(toolsLayout);
    toolsLayout->setSize("100%", 32);

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
    toolsLayout->addSpace(0.6f);

    // Add a blank row
    auto spacer = tgui::Panel::create();
    spacer->setSize("100%", 15);
    verticalLayout->add(spacer);

    // Collect unlocked prototypes and sort by toolboxOrder
    std::vector<const OT::Item::AbstractPrototype*> allProtos;
    for (const auto& prototype : game->itemFactory.prototypes) {
        if (prototype->unlockRating <= game->rating) {
            allProtos.push_back(prototype);
        }
    }
    std::sort(allProtos.begin(), allProtos.end(), [](const OT::Item::AbstractPrototype* a, const OT::Item::AbstractPrototype* b) {
        return a->toolboxOrder < b->toolboxOrder;
    });
    // Add buttons in rows of 3
    size_t numButtons = allProtos.size();
    size_t buttonsPerRow = 3;
    size_t numRows = (numButtons + buttonsPerRow - 1) / buttonsPerRow;
    for (size_t row = 0; row < numRows; ++row) {
        auto rowLayout = tgui::HorizontalLayout::create();
        verticalLayout->add(rowLayout);
        rowLayout->setSize(tgui::Layout2d("100%", 48));
        rowLayout->addSpace(0.5f);
        for (size_t col = 0; col < buttonsPerRow; ++col) {
            size_t index = row * buttonsPerRow + col;
            if (index >= numButtons) break;
            const auto* prototype = allProtos[index];
            std::string toolname = "item-" + prototype->id;
            auto button = makeButton(32, game->app.bitmaps["simtower/ui/toolbox/items"], prototype->icon);
            button->onPress([this, toolname](){ selectItem(toolname); });
            rowLayout->add(button);
            if (col < buttonsPerRow - 1) {
                rowLayout->addSpace(0.5f);
            }
        }
        rowLayout->addSpace(0.5f);
    }
    verticalLayout->addSpace(.5f);

    

    // Adjust window height based on content
    int totalHeight = 32 + 15 + static_cast<int>(numRows) * 48 + 32;
    totalHeight = std::max(totalHeight, 150); // Minimum height
    window->setClientSize({180, totalHeight});
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
    float scale = 1.5f;
    int scaledSize = static_cast<int>(size * scale);

    // Extract the sub-rectangle from the spritesheet
    sf::IntRect rect(index * size, 0, size, size);
    sf::Image iconImage = textureMap.copyToImage();
    sf::Image subImage;
    subImage.create(size, size);
    subImage.copy(iconImage, 0, 0, rect, false);

    // Scale the image
    sf::Image scaledImage;
    scaledImage.create(scaledSize, scaledSize);
    for (int y = 0; y < scaledSize; ++y) {
        for (int x = 0; x < scaledSize; ++x) {
            int srcX = x * size / scaledSize;
            int srcY = y * size / scaledSize;
            scaledImage.setPixel(x, y, subImage.getPixel(srcX, srcY));
        }
    }

    // Create a new SFML texture from the scaled image
    sf::Texture buttonTexture;
    buttonTexture.loadFromImage(scaledImage);

    // Load into TGUI texture
    tgui::Texture tguiImage;
    tguiImage.loadFromPixelData(buttonTexture.getSize(), buttonTexture.copyToImage().getPixelsPtr());

    tgui::BitmapButton::Ptr b = tgui::BitmapButton::create();
    b->setSize(scaledSize, scaledSize);
    b->setImage(tguiImage);
    return b;
}