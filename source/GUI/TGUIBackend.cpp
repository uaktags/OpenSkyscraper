/* Copyright © 2025 Tim G */
#include "TGUIBackend.h"
#include <TGUI/TGUI.hpp>
#include <TGUI/Backend/SFML-Graphics.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <functional>
#include <iostream>
#include "../Logger.h"

#include "../Application.h"  // for App
#include "../Path.h"

extern OT::Application* App;

namespace OT
{
    TGUIBackend::TGUIBackend() : gui(nullptr), fontManager(nullptr) {}

    TGUIBackend::~TGUIBackend() {}

    void TGUIBackend::init(sf::RenderWindow& window)
    {
        // Initialize fontManager
        fontManager = &App->fonts;  // Assuming App->fonts is FontManager
    
        // Create SFML backend GUI
        gui = std::make_unique<tgui::Gui>(window);
    
        try
        {
            // We pass the filename directly to TGUI. This assumes the application's
            // working directory is set up correctly for TGUI to find the file.
            gui->setFont("Jura-Regular.ttf");
            LOG(DEBUG, "Set global TGUI font to 'Jura-Regular.ttf'");
        }
        catch (const tgui::Exception& e)
        {
            LOG(ERROR, "TGUI failed to load font 'Jura-Regular.ttf': %s", e.what());
        }
    }

    bool TGUIBackend::handleEvent(const sf::Event& event)
    {
        if (gui) {
            sf::Event sfmlEvent = event;
            using GuiType = tgui::Gui;
            GuiType* derivedGui = static_cast<GuiType*>(gui.get());
            return derivedGui->handleEvent(sfmlEvent);
        }
        return false;
    }

    void TGUIBackend::render(sf::RenderWindow& window)
    {
        if (gui) {
            gui->draw();
        } else {
            LOG(ERROR, "TGUI Backend not initialized");
        }
    }

    void TGUIBackend::add(tgui::Widget::Ptr widget)
    {
        if (gui) {
            gui->add(widget);
        }
    }

    tgui::Button::Ptr TGUIBackend::createButton(const std::string& text)
    {
        return tgui::Button::create(text);
    }

    void TGUIBackend::setCallback(tgui::Button::Ptr btn, std::function<void()> cb)
    {
        if (btn) {
            btn->onClick(cb);
        }
    }

    tgui::Widget::Ptr TGUIBackend::createWidget(const std::string& type, const std::string& text)
    {
        if (type == "Button") {
            return createButton(text);
        }
        LOG(ERROR, "Unknown widget type: %s", type.c_str());
        return nullptr;
    }

    void TGUIBackend::setCallback(tgui::Widget::Ptr widget, std::function<void()> cb)
    {
        if (!widget) return;

        if (widget->getWidgetType() == "Button") {
            auto button = std::static_pointer_cast<tgui::Button>(widget);
            setCallback(button, cb);
        } else {
            LOG(ERROR, "setCallback only supported for Button");
        }
    }

    tgui::BackendGui* TGUIBackend::getGui()
    {
        return gui.get();
    }

    float TGUIBackend::getUIScale() const
    {
        return uiScale;
    }

    void TGUIBackend::setUIScale(float scale)
    {
        uiScale = scale;
    }
}