#pragma once

#include <functional>
#include <memory>
#include <string>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Window/Event.hpp>
#include <TGUI/TGUI.hpp>

namespace OT
{
    class FontManager;

    class IGUIRenderer
    {
    public:
        virtual ~IGUIRenderer() {}
        virtual void render(sf::RenderWindow& window) = 0;
    };

    class IGUIEventHandler
    {
    public:
        virtual ~IGUIEventHandler() {}
        virtual bool handleEvent(const sf::Event& event) = 0;
    };

    class IFontProvider
    {
    public:
        virtual ~IFontProvider() {}
    };

    class IGUIContext
    {
    public:
        virtual ~IGUIContext() {}
    };

    class IGUIBackend : public IGUIRenderer, public IGUIEventHandler, public IFontProvider, public IGUIContext
    {
    public:
        virtual ~IGUIBackend() {}
        virtual void init(sf::RenderWindow& window) = 0;

        virtual void add(tgui::Widget::Ptr widget) = 0;

        // TGUI-specific widgets
        virtual tgui::Button::Ptr createButton(const std::string& text) = 0;
        virtual void setCallback(tgui::Button::Ptr btn, std::function<void()> cb) = 0;
        virtual tgui::Widget::Ptr createWidget(const std::string& type, const std::string& text) = 0;
        virtual void setCallback(tgui::Widget::Ptr widget, std::function<void()> cb) = 0;
        virtual tgui::BackendGui* getGui() = 0;
        virtual float getUIScale() const = 0;
        virtual void setUIScale(float scale) = 0;
    };
}