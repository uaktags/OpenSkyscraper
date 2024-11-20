#include "TimeWindow.h"
#include "GameObject.h"
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include "Application.h"

using namespace OT;

TimeWindow::TimeWindow(Game * game) : GameObject(game) {
    window = tgui::ChildWindow::create();
    window->getRenderer()->setTitleBarHeight(10);
    
    window->setClientSize({431, 41});
    window->setPosition({200, 22});
    
    
    
    tgui::Texture image = tgui::Texture();
    image.load(app->bitmaps["simtower/ui/time/bg"]);
    
   auto picture = tgui::Picture::create(image);
   picture->setSize("100%", "100%");
   window->add(picture);
   
    
    
    app->gui.add(window);
}

void TimeWindow::close() {

}