#include "ToolboxWindow.h"
#include "GameObject.h"
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include "Application.h"

using namespace OT;

ToolboxWindow::ToolboxWindow(Game * game) : GameObject(game) {
    auto window = tgui::ChildWindow::create();
    auto renderer = window->getRenderer();
    renderer->setTitleBarHeight(10);
    
    window->setClientSize({128, 200});
    window->setPosition({0, 330});
    
    auto button = tgui::BitmapButton::create();
    button->setPosition({10, 10});
    button->setSize({21, 21});
    
    tgui::Texture image = tgui::Texture();
    image.load(app->bitmaps["simtower/ui/toolbox/tools"], tgui::UIntRect(0,0,21,21), tgui::UIntRect(0,0,21,21));
    
    button->setImage(image);
    window->add(button);
    
    
    app->gui.add(window);
}

void ToolboxWindow::close() {

}