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
    window = tgui::ChildWindow::create();
    window->getRenderer()->setTitleBarHeight(10);
    
    window->setClientSize({106, 220});
    window->setPosition({0, 330});
    
    reload();
    app->gui.add(window);
}

void ToolboxWindow::close() {

}

void ToolboxWindow::reload() {
    window->removeAllWidgets();
    //speed buttons

    // tool buttons
    auto bulldozeButton = makeButton(21, app->bitmaps["simtower/ui/toolbox/tools"], 0);
    bulldozeButton->setPosition({10, 10});
    window->add(bulldozeButton);

    auto fingerButton = makeButton(21, app->bitmaps["simtower/ui/toolbox/tools"], 1);
    fingerButton->setPosition({31, 10});
    window->add(fingerButton);

    auto inspectButton = makeButton(21, app->bitmaps["simtower/ui/toolbox/tools"], 2);
    inspectButton->setPosition({52, 10});
    window->add(inspectButton);

    // item buttons
    int row = 0;
    for (int i = 0; i < game->itemFactory.prototypes.size(); i++) {
		Item::AbstractPrototype * prototype = game->itemFactory.prototypes[i];

		char toolname[128];
		snprintf(toolname, 128, "item-%s", prototype->id.c_str());

		char style[512];
		snprintf(style, 512, "button#%s { background-image-s: %ipx %ipx; }", toolname, prototype->icon*32, prototype->icon*32+32);
		LOG(DEBUG, "style for %s: %s", prototype->name.c_str(), style);

        auto button = makeButton(32, app->bitmaps["simtower/ui/toolbox/items"], prototype->icon);
        int xpos = 5 + (32 * (i % 3));
        int ypos = 45 + row * 32;

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
    tgui::Texture image;
    image.load(textureMap, tgui::UIntRect(index*size,0,size,size), tgui::UIntRect(0,0,size,size));
    tgui::BitmapButton::Ptr b = tgui::BitmapButton::create();
    b->setSize(size, size);
    b->setImage(image);
    return b;
}