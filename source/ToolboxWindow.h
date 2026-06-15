/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once
#include "GameObject.h"
#include <TGUI/Widget.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Button.hpp>
#include <map>
#include <set>
#include <string>


namespace OT {
    class ToolboxWindow : public GameObject {
        public:
            ToolboxWindow(Game * game);
            ~ToolboxWindow() { close(); }

            void close();
            void reload();

            void updateTool();
            void updateSpeed();

            void onToolButtonPress(const char * tool);
            void onSpeedButtonPress(int speedMode);

            struct ButtonState {
                tgui::Texture normal;
                tgui::Texture hover;
                tgui::Texture checked;
            };

            tgui::Button::Ptr makeButton(int width, int height, sf::Texture textureMap, int index, ButtonState & state);

            tgui::ChildWindow::Ptr window = NULL;

            typedef std::set<tgui::Widget::Ptr> WidgetSet;
            WidgetSet buttons;

            typedef std::map<std::string, tgui::Button::Ptr> ToolButtonMap;
            ToolButtonMap toolButtons;

            typedef std::map<int, tgui::Button::Ptr> SpeedButtonMap;
            SpeedButtonMap speedButtons;

            std::map<tgui::Button::Ptr, ButtonState> buttonStates;

            int lastSpeedMode = 1;

    };
}
