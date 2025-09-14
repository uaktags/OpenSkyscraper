#pragma once
#include <TGUI/TGUI.hpp>
#include <TGUI/Widgets/BitmapButton.hpp>
#include "GameObject.h"
#include <chrono>
#include <set>
#include <string>
namespace OT {
	
	class ToolboxWindow : public GameObject
	{
	public:
		ToolboxWindow(Game * game);
		~ToolboxWindow() { close(); }
		
		void close();
		void reload();
		void advance(double dt);
		void updateAvailability();
		
		tgui::ChildWindow::Ptr window;
		std::set<tgui::BitmapButton::Ptr> buttons;
		
		void updateSpeed();
		void updateTool();
 
	private:
		// For handling press-and-hold
		std::chrono::steady_clock::time_point mouseDownTime{};
		bool longPressTriggered = false;
		tgui::Widget::Ptr pressedElement = nullptr;
		bool ignoreNextClick = false;
 
		void handleMouseDown(tgui::Widget::Ptr element);
		void handleMouseUp(tgui::Widget::Ptr element);
		void handleClick(tgui::Widget::Ptr element);
		void handleMouseMove();
		void triggerLongPress(tgui::Widget::Ptr element);
		void restoreOriginalLayout();
		void startLongPressTimer(tgui::Widget::Ptr element);
		void cancelLongPressTimer();
		bool isContextMenuOpen();
		void dismissContextMenu();
		void handleContextMenuClick(tgui::Widget::Ptr element);
		void selectItem(const std::string& id);
		tgui::BitmapButton::Ptr makeButton(int size, const sf::Texture& textureMap, int index);
	};
}
