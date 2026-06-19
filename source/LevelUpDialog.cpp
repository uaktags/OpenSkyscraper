/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Phase 3.2 - Level-up modal dialog. */
#include "LevelUpDialog.h"

#include "Application.h"
#include "Game.h"
#include "Item/Factory.h"
#include "Item/Prototype.h"
#include "LevelUp.h"

#include <sstream>

using namespace OT;

LevelUpDialog::LevelUpDialog(Game * game)
: GameObject(game)
{
	window   = nullptr;
	heading  = nullptr;
	unlocks  = nullptr;
	okButton = nullptr;
}

void LevelUpDialog::close()
{
	if (window)
	{
		app->gui.remove(window);
		window.reset();
		heading.reset();
		unlocks.reset();
		okButton.reset();
	}
}

std::string LevelUpDialog::unlockedAt(int rating) const
{
	// Build the comma-separated list of prototype display names that
	// require exactly this rating. Names are pulled from the factory so
	// we honour whatever the loaded prototypes call themselves.
	std::ostringstream out;
	bool first = true;
	for (std::vector<Item::AbstractPrototype *>::const_iterator it = game->itemFactory.prototypes.begin();
	     it != game->itemFactory.prototypes.end(); ++it)
	{
		Item::AbstractPrototype * p = *it;
		if (!p) continue;
		if (LevelUp::minRatingToBuild(p->id) != rating) continue;
		if (!first) out << ", ";
		out << p->name;
		first = false;
	}
	return out.str();
}

void LevelUpDialog::buildWindow()
{
	window = tgui::ChildWindow::create();
	window->setTitle("Promotion!");
	auto renderer = window->getRenderer();
	renderer->setTitleBarHeight(16 * app->uiScale);
	renderer->setBackgroundColor(sf::Color(35, 40, 55, 240));
	renderer->setTitleBarColor(sf::Color(70, 110, 170));
	renderer->setBorderColor(sf::Color(120, 150, 200));
	renderer->setBorders(2);

	window->setClientSize({260 * app->uiScale, 150 * app->uiScale});

	// Centre on the viewport. The time window lives at the top and the
	// toolbox on the left, so a centred-ish position keeps this out of
	// the way without overlapping the inspector popup (top-left).
	auto sz = app->window.getSize();
	window->setPosition({static_cast<float>(sz.x) / 2.f - 130 * app->uiScale,
	                     static_cast<float>(sz.y) / 2.f - 75 * app->uiScale});

	heading = tgui::Label::create();
	heading->setTextSize(14 * app->uiScale);
	heading->setPosition({12 * app->uiScale, 8 * app->uiScale});
	heading->getRenderer()->setTextColor(sf::Color(255, 230, 120));
	window->add(heading);

	unlocks = tgui::Label::create();
	unlocks->setTextSize(11 * app->uiScale);
	unlocks->setPosition({12 * app->uiScale, 36 * app->uiScale});
	unlocks->getRenderer()->setTextColor(sf::Color(220, 226, 234));
	unlocks->setSize({236 * app->uiScale, 80 * app->uiScale});
	window->add(unlocks);

	okButton = tgui::Button::create("OK");
	okButton->setSize(70 * app->uiScale, 22 * app->uiScale);
	okButton->setPosition({94 * app->uiScale, 122 * app->uiScale});
	okButton->setTextSize(12 * app->uiScale);
	okButton->onPress([this] { close(); });
	window->add(okButton);

	// Tear down when the user clicks the title-bar close widget.
	window->onClose([this] { close(); });

	app->gui.add(window);
}

void LevelUpDialog::showForRating(int newRating)
{
	close();
	buildWindow();

	const int stars = newRating + 1; // rating is 0-based
	std::ostringstream h;
	h << "Congratulations - your tower reached " << stars << " stars!";
	heading->setText(h.str());

	const std::string list = unlockedAt(newRating);
	std::ostringstream u;
	if (list.empty())
		u << "New facilities are now available.";
	else
		u << "New facilities unlocked:\n" << list;
	unlocks->setText(u.str());
}
