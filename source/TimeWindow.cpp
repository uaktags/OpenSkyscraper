#include "TimeWindow.h"
#include "GameObject.h"
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include "Application.h"

using namespace OT;

TimeWindow::TimeWindow(Game * game) : GameObject(game) {
    auto window = tgui::ChildWindow::create();
    auto renderer = window->getRenderer();
    renderer->setTitleBarHeight(10);
    
    window->setClientSize({431, 41});
    window->setPosition({200, 22});
    
    
    
    tgui::Texture image = tgui::Texture();
    image.load(app->bitmaps["simtower/ui/toolbox/tools"], tgui::UIntRect(0,0,21,21), tgui::UIntRect(0,0,21,21));
    
   
    
    
    app->gui.add(window);
}

void TimeWindow::close() {

}