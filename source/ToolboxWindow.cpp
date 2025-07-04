#include "ToolboxWindow.h"
#include "GameObject.h"
#include <SFML/Graphics/Texture.hpp>
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/TwoFingerScrollDetect.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include "Application.h"
#include "Game.h"

using namespace OT;

ToolboxWindow::ToolboxWindow(Game * game) : GameObject(game) {
    // No need to explicitly load, BitmapManager will handle it on first use
    window = tgui::ChildWindow::create();
    window->getRenderer()->setTitleBarHeight(10 * app->uiScale);
    window->setClientSize({106 * app->uiScale, app->uiScale * 220});
    // Move toolbox window up to just below the menu bar (22px * uiScale)
    window->setPosition({0, 24 * app->uiScale});
    
    reload();
    app->gui.add(window);
}

void ToolboxWindow::close() {

}

void ToolboxWindow::reload() {
    window->removeAllWidgets();

    auto topLayout = tgui::VerticalLayout::create();
    window->add(topLayout);
    topLayout->setSize("100%", "100%");

    // tool buttons
    auto toolsLayout = tgui::HorizontalLayout::create();
    topLayout->add(toolsLayout, "toolsLayout");
    toolsLayout->setSize("100%", 21 * app->uiScale);

    toolsLayout->addSpace(0.6f);
    auto bulldozeButton = makeButton(21, app->bitmaps["simtower/ui/toolbox/tools"], 0);
    toolsLayout->add(bulldozeButton);

    auto fingerButton = makeButton(21, app->bitmaps["simtower/ui/toolbox/tools"], 1);
    toolsLayout->add(fingerButton);

    auto inspectButton = makeButton(21, app->bitmaps["simtower/ui/toolbox/tools"], 2);
    toolsLayout->add(inspectButton);
    toolsLayout->addSpace(0.6f);

    // item buttons
    int row = 0;
    for (int i = 0; i < game->itemFactory.prototypes.size(); i++) {
        Item::AbstractPrototype * prototype = game->itemFactory.prototypes[i];

        char toolname[128];
        snprintf(toolname, 128, "item-%s", prototype->id.c_str());

        char style[512];
        snprintf(style, 512, "button#%s { background-image-s: %ipx %ipx; }", toolname, prototype->icon*32, prototype->icon*32+32);
        LOG(DEBUG, "style for %s: %s", prototype->name.c_str(), style);
        LOG(DEBUG, "prototype->icon for %s: %d", prototype->name.c_str(), prototype->icon);

        // Choose spritesheet based on uiScale
        sf::Texture itemTexture;
        //if (app->uiScale == 1.0f) {
            itemTexture = app->bitmaps["simtower/ui/toolbox/items"];
        //} else {
        //    itemTexture = app->bitmaps["toolbox/grey.png"];
        //}

        auto button = makeButton(32, itemTexture, prototype->icon);
        int xpos = 5 + (32 * (i % 3)) * app->uiScale;
        int ypos = 45 + row * 32 * app->uiScale;

        button->setPosition(xpos, ypos);
        window->add(button);
        buttons.insert(button);
        button->onPress(&ToolboxWindow::onToolButtonPress, this, toolname);
        if (i % 3 >= 2)
            row++;
    }
}

void ToolboxWindow::onToolButtonPress(const char * tool) {
    LOG(IMPORTANT, "tool button pressed: %s", tool);
    game->selectTool(tool);
}

tgui::BitmapButton::Ptr ToolboxWindow::makeButton(int size, sf::Texture textureMap, int index) {
    int scaledSize = size * app->uiScale;

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

    // Load into TGUI texture using loadFromPixelData
    tgui::Texture tguiImage;
    tguiImage.loadFromPixelData({scaledSize, scaledSize}, scaledImage.getPixelsPtr());

    tgui::BitmapButton::Ptr b = tgui::BitmapButton::create();
    b->setSize(scaledSize, scaledSize);
    b->setImage(tguiImage);
    return b;
}