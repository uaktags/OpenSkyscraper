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
#include <vector>
#include <chrono>


namespace OT {
    namespace Item { class AbstractPrototype; }

    class ToolboxWindow : public GameObject {
        public:
            ToolboxWindow(Game * game);
            ~ToolboxWindow() { close(); }

            void close();
            void reload();

            void update();         ///< per-frame: drives the press-and-hold overlay
            void updateTool();
            void updateSpeed();

            void onToolButtonPress(const char * tool);
            void onSpeedButtonPress(int speedMode);

            struct ButtonState {
                tgui::Texture normal;
                tgui::Texture hover;
                tgui::Texture checked;
            };

            tgui::Button::Ptr makeButton(int width, int height, sf::Texture textureMap, int index, ButtonState & state, int cellW = 0, int cellH = 0);

            tgui::ChildWindow::Ptr window = NULL;

            typedef std::set<tgui::Widget::Ptr> WidgetSet;
            WidgetSet buttons;

            typedef std::map<std::string, tgui::Button::Ptr> ToolButtonMap;
            ToolButtonMap toolButtons;

            typedef std::map<int, tgui::Button::Ptr> SpeedButtonMap;
            SpeedButtonMap speedButtons;

            tgui::Button::Ptr mapButton    = nullptr; ///< minimap toggle (mirrors the M keybind)
            tgui::Button::Ptr statusButton = nullptr; ///< cycles status overlay mode (mirrors the O keybind)
            tgui::Button::Ptr financeButton = nullptr; ///< finance window toggle (mirrors the F keybind)

            std::map<tgui::Button::Ptr, ButtonState> buttonStates;

            int lastSpeedMode = 1;

            bool isChildTool(const std::string& toolId, std::string& outParentId) const;
            bool isCategoryParent(const std::string& toolId) const;

        private:
            tgui::Button::Ptr makeItemButton(Item::AbstractPrototype* proto, ButtonState& state);
            void showOverlayFor(const std::string& parentId);
            void hideOverlay();
            void rebuildSlotTexture(const std::string& parentId, const std::string& displayedProtoId);

            // Press-and-hold overlay state. While a category parent button is
            // held down its children pop up over the neighbouring grid slots;
            // releasing over a child selects it. Release is detected by
            // polling the mouse button in update() rather than relying on
            // TGUI mouse capture (which is unreliable with stacked widgets).
            std::string holdingParent;                          ///< proto id of the parent being held, or empty
            std::chrono::steady_clock::time_point holdPressTime;
            bool overlayVisible = false;
            std::vector<tgui::Button::Ptr> overlayButtons;
            std::map<tgui::Button::Ptr, ButtonState> overlayButtonStates;
            std::map<tgui::Button::Ptr, std::string> overlayToolFor;   ///< overlay button -> "item-<childId>"
            std::map<std::string, int> parentSlotIndex;                ///< parent proto id -> grid slot

            // Slot substitution. When a category child is selected via the
            // overlay, the parent slot's icon is replaced with the child's
            // (e.g. picking Floor replaces Lobby's icon with Floor's). The
            // slot stays in toolButtons under the parent's id but its
            // ButtonState is swapped to the child's textures. Empty value
            // means the slot is showing its parent (the default).
            std::map<std::string, std::string> parentActiveChild;       ///< parent proto id -> displayed child proto id
    };
}
