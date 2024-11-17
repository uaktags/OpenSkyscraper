#pragma once
#include "GameObject.h"
#include <TGUI/Widget.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>

namespace OT {
    class ToolboxWindow : public GameObject {
        public:
            ToolboxWindow(Game * game);
            ~ToolboxWindow() { close(); }

            void close();

            tgui::ChildWindow * window;

            typedef std::set<tgui::Widget *> WidgetSet;
            WidgetSet buttons;
            
    };
}