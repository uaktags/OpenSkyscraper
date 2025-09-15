#include <cassert>

#include "Application.h"
#include "GUIManager.h"

using namespace OT;

GUIManager::GUIManager()
{
	window = NULL;
	backend = nullptr;
}

GUIManager::~GUIManager()
{
	// Cleanup is handled by unique_ptr RAII
}

bool GUIManager::init(sf::RenderWindow * window)
{
	assert(window != NULL && "window must not be NULL");

	this->window = window;
	backend = std::make_unique<TGUIBackend>();
	backend->init(*window);
	return true;
}

IGUIBackend* GUIManager::getBackend()
{
	return backend.get();
}

float GUIManager::getUIScale() const
{
	return backend->getUIScale();
}

void GUIManager::setUIScale(float scale)
{
	backend->setUIScale(scale);
}
