#pragma once
#include "GameObject.h"
#include <TGUI/Widget.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>


namespace OT {
    class ToolboxWindow : public GameObject {
        public:
            ToolboxWindow(Game * game);
            ~ToolboxWindow() { close(); }

            void close();
            void reload();

            tgui::BitmapButton::Ptr makeButton(int size, sf::Texture textureMap, int index);

            tgui::ChildWindow::Ptr window = NULL;

            typedef std::set<tgui::Widget *> WidgetSet;
            WidgetSet buttons;
            
    };
}