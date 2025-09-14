#pragma once

#include <memory>
#include <SFML/Graphics/RenderWindow.hpp>

#include "GUI/TGUIBackend.h"

namespace OT
{
	class GUI;

	class GUIManager
	{
		friend class GUI;

	public:
		GUIManager();
		~GUIManager();

		bool init(sf::RenderWindow * window);

		IGUIBackend* getBackend();

		float getUIScale() const;
		void setUIScale(float scale);

	protected:
		sf::RenderWindow * window;

	private:
		std::unique_ptr<TGUIBackend> backend;
	};
}
