#include <cassert>
#include <cmath>
#include <algorithm>
#ifdef BUILD_DEBUG
#include <Rocket/Debugger/Debugger.h>
#endif

#include "Application.h"
#include "GUI.h"
#include "GUIManager.h"
#include "OpenGL.h"

using namespace OT;


GUI::GUI(std::string name, GUIManager * manager)
{
	assert(manager && "GUI requires a GUIManager");

	this->manager = manager;
	// Default UI scale
	this->uiScale = 1.0f;
	// Use the actual window pixel size for the GUI context. Using the view
	// size caused the Rocket context to operate in world/view coordinates
	// which desynchronized GUI mouse coordinates (crosshair) from the
	// SFML mouse position when the view/zoom changed or the window was
	// resized. getSize() returns the window size in pixels.
	unsigned width = manager->window->getSize().x;
	unsigned height = manager->window->getSize().y;
	// Apply UI scale to context dimensions so Rocket lays out documents
	// at the desired scaled size. Rocket expects integer pixel sizes.
	unsigned ctx_w = (unsigned)std::max<int>(1, (int)round(width / uiScale));
	unsigned ctx_h = (unsigned)std::max<int>(1, (int)round(height / uiScale));
	context = Rocket::Core::CreateContext(name.c_str(), Rocket::Core::Vector2i(ctx_w, ctx_h));
	assert(context && "unable to initialize context");
}

GUI::~GUI()
{
	context->RemoveReference();
	context = NULL;
}

bool GUI::handleEvent(sf::Event & event)
{
	switch (event.type) {
		case sf::Event::Resized:
			{
				// The resize event gives the new window dimensions in pixels. Scale
				// them by uiScale so the Rocket context dimensions remain consistent
				// with the chosen UI scaling.
				unsigned ctx_w = (unsigned)std::max<int>(1, (int)std::round(event.size.width / uiScale));
				unsigned ctx_h = (unsigned)std::max<int>(1, (int)std::round(event.size.height / uiScale));
				this->context->SetDimensions(Rocket::Core::Vector2i(ctx_w, ctx_h));
			}
			return true;
		case sf::Event::MouseMoved:
			{
				// Scale mouse coordinates down to Rocket's context coordinate system
				// if UI scaling is active.
				int mx = (int)std::round(event.mouseMove.x / uiScale);
				int my = (int)std::round(event.mouseMove.y / uiScale);
				context->ProcessMouseMove(mx, my, manager->getKeyModifiers());
			}
			return true;
		case sf::Event::MouseButtonPressed:
			// Mouse button coordinates aren't required by Rocket here, but if
			// other systems rely on event positions when UI is scaled they will
			// receive the raw event; Rocket gets logical coordinates via
			// ProcessMouseMove() above.
			context->ProcessMouseButtonDown(event.mouseButton.button, manager->getKeyModifiers());
			return true;
		case sf::Event::MouseButtonReleased:
			context->ProcessMouseButtonUp(event.mouseButton.button, manager->getKeyModifiers());
			return true;
		case sf::Event::MouseWheelMoved:
			return context->ProcessMouseWheel(event.mouseWheel.delta, manager->getKeyModifiers());
		case sf::Event::TextEntered:
			return context->ProcessTextInput(event.text.unicode);
		case sf::Event::KeyPressed:
			return context->ProcessKeyDown(manager->translateKey(event.key.code), manager->getKeyModifiers());
		case sf::Event::KeyReleased:
			return context->ProcessKeyUp(manager->translateKey(event.key.code), manager->getKeyModifiers());
	}
	return false;
}

void GUI::draw()
{
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	// Render Rocket in window pixel coordinates. Using the view size here
	// also caused drawing and mouse coordinates to be offset when the
	// view was changed (e.g. zoom or camera adjustments).
	unsigned width = manager->window->getSize().x;
	unsigned height = manager->window->getSize().y;
	// Apply UI scale by scaling the projection. We render Rocket at a
	// scaled size so that elements appear larger/smaller while keeping
	// input coordinates consistent with the scaled layout.
	float s = uiScale;
	glOrtho(0.0, width / s, height / s, 0.0, -1.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	// Scale modelview so Rocket's rendering is drawn at the desired size.
	glScalef(s, s, 1.0f);

	context->Update();
	context->Render();

	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void GUI::setUIScale(float scale)
{
	if (scale <= 0.0f) return;
	if (std::fabs(scale - uiScale) < 1e-6f) return;
	uiScale = scale;
	// Update context dimensions to reflect new UI scale using current
	// window size.
	unsigned width = manager->window->getSize().x;
	unsigned height = manager->window->getSize().y;
	unsigned ctx_w = (unsigned)std::max<int>(1, (int)std::round(width / uiScale));
	unsigned ctx_h = (unsigned)std::max<int>(1, (int)std::round(height / uiScale));
	this->context->SetDimensions(Rocket::Core::Vector2i(ctx_w, ctx_h));
}

float GUI::getUIScale() const { return uiScale; }

Rocket::Core::ElementDocument * GUI::loadDocument(Path path)
{
	DataManager::Paths paths = App->data.paths(Path("gui").down(path));
	for (DataManager::Paths::iterator p = paths.begin(); p != paths.end(); p++) {
		Rocket::Core::ElementDocument * document = context->LoadDocument((*p).c_str());
		if (document) {
			LOG(DEBUG, "loaded GUI document '%s'", path.c_str());
			return document;
		}
	}
	LOG(ERROR, "unable to load GUI document '%s'", path.c_str());
	return NULL;
}
