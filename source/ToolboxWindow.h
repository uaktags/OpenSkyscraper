#pragma once
#include <Rocket/Core/ElementDocument.h>
#include <Rocket/Core/EventListener.h>
#include "GameObject.h"
#include <chrono>
namespace OT {
	
	class ToolboxWindow : public GameObject, private Rocket::Core::EventListener
	{
	public:
		ToolboxWindow(Game * game) : GameObject(game) {
			window = NULL;
		}
		~ToolboxWindow() { close(); }
		
		void close();
		void reload();
		void updateAvailability();
		void advance(double dt);
		
		Rocket::Core::ElementDocument * window;
		typedef std::set<Rocket::Core::Element *> ElementSet;
		ElementSet buttons;
		
		void ProcessEvent(Rocket::Core::Event & event);
		
		void updateSpeed();
		void updateTool();

	private:
		// For handling press-and-hold
		std::chrono::steady_clock::time_point mouseDownTime{};
		bool longPressTriggered = false;
		Rocket::Core::Element* pressedElement = nullptr;
		// When a long-press is released we may handle the selection on mouseup
		// and need to ignore the subsequent click event. This flag suppresses
		// the next click when set.
		bool ignoreNextClick = false;

		void handleMouseDown(Rocket::Core::Event &event);
		void handleMouseUp(Rocket::Core::Event &event);
		void handleClick(Rocket::Core::Event &event);
		void handleMouseMove(Rocket::Core::Event &event);
		void triggerLongPress(Rocket::Core::Element* element);
		void restoreOriginalLayout();
		void startLongPressTimer(Rocket::Core::Element* element);
		void cancelLongPressTimer();
		bool isContextMenuOpen();
		void dismissContextMenu();
		void handleContextMenuClick(Rocket::Core::Element* element);
		void selectItem(const std::string& id);
	};
}
