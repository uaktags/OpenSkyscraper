#pragma once
#include "State.h"
#include "Application.h"


namespace OT {
    class MainMenu : public State {
        public:
            MainMenu(Application & app);
            ~MainMenu();

            void activate();
            bool handleEvent(sf::Event & event);
            void advance(double dt);
            void deactivate();
            void onNewGamePress();

        private:
            Application & app;
            
            tgui::Panel::Ptr panel = NULL;
            tgui::Button::Ptr newButton = NULL;
            tgui::Button::Ptr loadButton = NULL;
            tgui::Button::Ptr quitButton = NULL;
            tgui::Picture::Ptr bgPicture = NULL;
    };
}