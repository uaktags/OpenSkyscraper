/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Phase 3.2 - Level-up modal dialog. */
#include "LevelUpDialog.h"

#include "Application.h"
#include "Game.h"
#include "Item/Factory.h"
#include "Item/Prototype.h"
#include "LevelUp.h"
#include "PluginManager.h"
#include "YootWorld.h"

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
		anim.reset();
	}
	animFrames.clear();
}

void LevelUpDialog::advance(double dt)
{
	if (!window || !anim || animFrames.empty()) return;
	animClock += dt;
	if (animClock < 0.1) return;
	animClock = 0;
	if (++animFrame >= (int)animFrames.size()) animFrame = std::min(animLoop, (int)animFrames.size() - 1);
	anim->getRenderer()->setTexture(animFrames[animFrame]);
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
		if (game->activeRules.minRatingToBuild(p->id) != rating) continue;
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

	// YootTower: the grade row (new grade g uses row g - 2) names the
	// animation bitmap and its loop frame (Tokyo 1207..1210, loop 8 / 6).
	YootWorld::Id world;
	const int grade = newRating + 1;
	if (!game->activeRules.profile().yootWorld || !parseYootWorldIdentifier(game->activeRules.profile().worldId, world) || grade < 2)
		return;
	YootWorld w;
	AbstractPlugin * plugin = app->pluginManager.get(yootWorldPluginFile(world));
	if (!plugin || !plugin->ensureLoaded() || !loadYootWorld(*plugin, world, w)) return;
	if (grade - 2 >= (int)w.grades.size() || w.grades[grade - 2].animId <= 0) return;
	sf::Texture sheet;
	if (!app->bitmaps.loadFromPlugin(yootWorldPluginFile(world), w.grades[grade - 2].animId, sheet)) return;
	const sf::Image img = sheet.copyToImage();
	const unsigned fw = 144, fh = 96;
	if (img.getSize().x < fw || img.getSize().y < fh) return;
	const int frames = w.grades[grade - 2].animFrames > 0 ? w.grades[grade - 2].animFrames : 1000;
	for (unsigned y = 0; y + fh <= img.getSize().y && (int)animFrames.size() < frames; y += fh)
	{
		sf::Image frame({fw, fh});
		if (!frame.copy(img, {0, 0}, sf::IntRect({0, (int)y}, {(int)fw, (int)fh}))) break;
		tgui::Texture t;
		t.loadFromPixelData(frame.getSize(), frame.getPixelsPtr());
		animFrames.push_back(t);
	}
	if (animFrames.empty()) return;
	animFrame = 0;
	animClock = 0;
	animLoop = std::max(0, w.grades[grade - 2].animParam);
	const float s = app->uiScale;
	window->setClientSize({260 * s, 250 * s});
	anim = tgui::Picture::create(animFrames[0]);
	anim->setSize({(float)fw * s, (float)fh * s});
	anim->setPosition({58 * s, 8 * s});
	window->add(anim);
	heading->setPosition({12 * s, 108 * s});
	unlocks->setPosition({12 * s, 136 * s});
	okButton->setPosition({94 * s, 222 * s});
}
