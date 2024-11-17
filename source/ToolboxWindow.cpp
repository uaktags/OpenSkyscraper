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
    
    window->setClientSize({128, 200});
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
    for (int i = 0; i < game->itemFactory.prototypes.size(); i++) {
		Item::AbstractPrototype * prototype = game->itemFactory.prototypes[i];

		char id[128];
		snprintf(id, 128, "item-%s", prototype->id.c_str());

		char style[512];
		snprintf(style, 512, "button#%s { background-image-s: %ipx %ipx; }", id, prototype->icon*32, prototype->icon*32+32);
		LOG(DEBUG, "style for %s: %s", prototype->name.c_str(), style);

        auto button = makeButton(32, app->bitmaps["simtower/ui/toolbox/items"], i);
        button->setPosition(10, 45+i*32);
        window->add(button);
	}
}

tgui::BitmapButton::Ptr ToolboxWindow::makeButton(int size, sf::Texture textureMap, int index) {
    tgui::Texture image;
    image.load(textureMap, tgui::UIntRect(index*size+index,0,size,size), tgui::UIntRect(0,0,size,size));
    tgui::BitmapButton::Ptr b = tgui::BitmapButton::create();
    b->setSize(size, size);
    b->setImage(image);
    return b;
}