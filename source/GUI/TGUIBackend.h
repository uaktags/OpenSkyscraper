/* Copyright © 2025 Tim G */
#pragma once

#include <memory>
#include <functional>
#include <string>
#include <TGUI/TGUI.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>

#include "IGUI.h"
#include "../FontManager.h"

namespace OT
{
    class TGUIBackend : public IGUIBackend
    {
    public:
        TGUIBackend();
        ~TGUIBackend() override;

        void init(sf::RenderWindow& window) override;
        bool handleEvent(const sf::Event& event) override;
        void render(sf::RenderWindow& window) override;
        void add(tgui::Widget::Ptr widget) override;
        tgui::Button::Ptr createButton(const std::string& text) override;
        void setCallback(tgui::Button::Ptr btn, std::function<void()> cb) override;
        tgui::Widget::Ptr createWidget(const std::string& type, const std::string& text) override;
        void setCallback(tgui::Widget::Ptr widget, std::function<void()> cb) override;
        tgui::BackendGui* getGui() override;

        float getUIScale() const;
        void setUIScale(float scale);

    private:
        std::unique_ptr<tgui::BackendGui> gui;
        FontManager* fontManager;
        float uiScale = 1.0f;
    };
}