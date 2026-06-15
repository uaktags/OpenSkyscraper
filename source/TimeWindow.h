#pragma once
#include "GameObject.h"
#include <TGUI/Rect.hpp>
#include <TGUI/Widget.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <SFML/System/Vector2.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Backend/Renderer/SFML-Graphics/CanvasSFML.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include <string>


namespace OT {
    class TimeWindow : public GameObject {
        public:
            TimeWindow(Game * game);
            ~TimeWindow() { close(); }

            void close();
            void reload();

            void updateTime();
            void updateRating();
            void updateFunds();
            void updatePopulation();
            void updateMoneyStats();
            void updateTooltip();
            void showMessage(std::string msg);

            void advance (double dt);
            void renderWatch();

            void setVisible(bool visible);
            bool isVisible() const;
            
        public:
            tgui::ChildWindow::Ptr window = NULL;

        private:
            static std::string formatMoney(int amount);
            static std::string formatCompactMoney(int amount);
            static std::string formatSignedCompactMoney(int amount);
            tgui::Label::Ptr lblPopulation = NULL;
            tgui::Label::Ptr lblFunds = NULL;
            tgui::Label::Ptr lblMoneyStats = NULL;
            tgui::Label::Ptr lblDate = NULL;
            tgui::Label::Ptr lblTooltip = NULL;
            tgui::Label::Ptr lblRating = NULL;
            tgui::Label::Ptr lblFundsTitle = NULL;
            tgui::Label::Ptr lblPopulationTitle = NULL;
            tgui::Texture rating_tx;
            tgui::Picture::Ptr rating;
            tgui::CanvasSFML::Ptr watch = NULL;

            double messageTimer;
            std::string message;
            int displayedSpeedMode = -1;
            sf::Vector2f watchPos {6, 6};
	        const sf::Vector2f watchSize {15, 15};
            tgui::UIntRect rating_rect;


            //tgui::ChildWindow * window;           
    };
}
