/* Copyright © 2013 Fabian Schuiki */
#include "../Game.h"
#include "../YootRules.h"
#include "../Math/Rand.h"
#include "Shop.h"
#include <string>
#include <algorithm>
#include <cmath>

using namespace OT;
using namespace OT::Item;

// Authentic tenant name pool (SIMTOWER.EXE string table res 0x2cc; matches
// the 11 storefront sheets 0x8668-0x8672 one-to-one).
const char * const Shop::kNameVariants[11] = {
	"Men's Clothing", "Pet Store", "Flower Shop", "Book Store",
	"Drug Store", "Boutique", "Electronics", "Bank", "Hair Salon",
	"Post Office", "Sports Gear"
};

Shop::~Shop()
{
}

void Shop::init()
{
	Item::init();
	variant = rand() % 11;
	displayName = kNameVariants[variant];
	let = false;
	open = false;
	patrons = 0;
	rentClass = 1;
	serviceScore[0] = serviceScore[1] = serviceScore[2] = 10;
	dailyQuota = quotaRemaining = walkinsToday = walkinsYesterday = customersToday = 0;
	lastDemandDay = -1;
	sprite.setTexture(App->bitmaps.editionTexture("shop", "simtower/shops"));
	sprite.setOrigin({0.f, 24.f});
	sprite.setPosition({static_cast<float>(getPosition().x * 8), static_cast<float>(-getPosition().y * 36)});
	addSprite(&sprite);
	spriteNeedsUpdate = false;
	updateSprite();
}

void Shop::encodeXML(tinyxml2::XMLPrinter &xml)
{
	Item::encodeXML(xml);
	xml.PushAttribute("variant", variant);
	xml.PushAttribute("let", let);
	xml.PushAttribute("open", open);
	xml.PushAttribute("patrons", patrons);
	xml.PushAttribute("rentClass", rentClass);
	xml.PushAttribute("dailyQuota", dailyQuota);
	xml.PushAttribute("quotaRemaining", quotaRemaining);
	xml.PushAttribute("walkinsToday", walkinsToday);
	xml.PushAttribute("walkinsYesterday", walkinsYesterday);
	xml.PushAttribute("customersToday", customersToday);
	xml.PushAttribute("lastDemandDay", lastDemandDay);
	for (int i = 0; i < 3; ++i) {
		char key[32]; snprintf(key, sizeof(key), "serviceScore%d", i);
		xml.PushAttribute(key, serviceScore[i]);
	}
}

void Shop::decodeXML(tinyxml2::XMLElement &xml)
{
	Item::decodeXML(xml);
	variant = xml.IntAttribute("variant", 0);
	if (variant < 0 || variant >= 11) variant = 0;
	displayName = kNameVariants[variant];
	let = xml.BoolAttribute("let", false);
	open = xml.BoolAttribute("open", false);
	patrons = std::max(0, std::min(40, xml.IntAttribute("patrons", 0)));
	rentClass = std::max(0, std::min(3, xml.IntAttribute("rentClass", 1)));
	dailyQuota = std::max(0, xml.IntAttribute("dailyQuota", 0));
	quotaRemaining = std::max(0, xml.IntAttribute("quotaRemaining", dailyQuota));
	walkinsToday = std::max(0, xml.IntAttribute("walkinsToday", 0));
	walkinsYesterday = std::max(0, xml.IntAttribute("walkinsYesterday", 0));
	customersToday = std::max(0, xml.IntAttribute("customersToday", 0));
	lastDemandDay = xml.IntAttribute("lastDemandDay", -1);
	for (int i = 0; i < 3; ++i) {
		char key[32]; snprintf(key, sizeof(key), "serviceScore%d", i);
		serviceScore[i] = std::max(0, xml.IntAttribute(key, 10));
	}
	updateSprite();
}

int Shop::stateByte() const
{
	if (!let) return 0xFF;
	if (!open) return 3;
	return patrons >= 10 ? 2 : patrons >= 1 ? 1 : 0;
}

void Shop::setLet(bool l)
{
	if (let == l) return;
	let = l;
	spriteNeedsUpdate = true;
}

void Shop::setOpen(bool o)
{
	if (open == o) return;
	open = o;
	spriteNeedsUpdate = true;
}

void Shop::setPatrons(int p)
{
	p = std::max(0, std::min(40, p));
	if (patrons == p) return;
	patrons = p;
	spriteNeedsUpdate = true;
}

void Shop::updateSprite()
{
	spriteNeedsUpdate = false;
	if (!let)
	{
		// Shared "For RENT" storefront (0x8673), merged-sheet row 0, x 0..96.
		sprite.setTextureRect(sf::IntRect({0, 0}, {96, 24}));
	}
	else if (!open)
	{
		// Shared shuttered storefront (0x8674), row 0, x 96..192.
		sprite.setTextureRect(sf::IntRect({96, 0}, {96, 24}));
	}
	else
	{
		// Fill tiers of the variant strip (rows 1..11 below the shared
		// row): frame 0 empty, 1 for 1-9 patrons, 2 for 10+ packed.
		int tier = stateByte();
		sprite.setTextureRect(sf::IntRect({tier * 96, 24 + variant * 24}, {96, 24}));
	}
}

void Shop::advance(double dt)
{
	(void)dt;
	const int day = static_cast<int>(std::floor(game->time.absolute));
	const int period = game->sky.rainyDay ? 2 : (game->time.isHoliday() ? 1 : 0);
	const int cap = period == 0 ? 25 : period == 1 ? 30 : 18;
	if (game->time.checkHour(10)) {
		open = true;
		patrons = 0;
		walkinsYesterday = walkinsToday;
		population = walkinsYesterday;
		customersToday = walkinsToday = 0;
		int score = serviceScore[period];
		if (period == 1) score = std::max(score, serviceScore[0]);
		dailyQuota = std::max(10, std::min(cap, score));
		quotaRemaining = dailyQuota;
		lastDemandDay = day;
		serviceScore[period] = 0;
		spriteNeedsUpdate = true;
	}
	if (open && quotaRemaining > 0 && patrons < 40 && game->time.checkTick(16.0 / 2600.0)) {
		const double hour = game->time.hour;
		int width = (hour >= 10.0 && hour < 17.0) ? 36 :
			(hour >= 17.0 && hour < 21.0) ? 6 : 0;
		if (width > 0 && (width == 1 || rand() % width == 0)) {
			if (!let) setLet(true); // first admitted walk-in re-lets a vacant shop
			++patrons;
			++walkinsToday;
			++customersToday;
			--quotaRemaining;
			serviceScore[period] = std::min(cap, serviceScore[period] + 2);
			spriteNeedsUpdate = true;
		}
	}
	if (game->time.checkHour(21) && open) {
		open = false;
		population = walkinsToday;
		patrons = 0;
		spriteNeedsUpdate = true;
		if (prototype->yoot && let) {
			// YootTower E6: shops earn CalcInMoney at close instead of rent.
			const YootItemStats &y = *prototype->yoot;
			game->yootBook(Yoot::commercialIncome(customersToday, y), "retail_income", "Income from Shop");
			yootEstimate = Yoot::profitEstimate(customersToday, y.inMoneyFor(0) / kYootMoneyUnit, y.outMoney / kYootMoneyUnit);
		}
	}
	if (spriteNeedsUpdate) updateSprite();
}

Path Shop::getRandomBackgroundSoundPath()
{
	if (!let || !open)
		return "";
	// The EXE alternates the shop ambience between the two waves fast food
	// also uses (concilia audio_events.h: AMB_RESTAURANT_B 0x8569 and
	// AMB_SHOP_FF_B 0x8668), loaded as simtower/fastfood/0 and /2.
	char name[128];
	snprintf(name, 128, "simtower/fastfood/%i", (rand() & 1) ? 0 : 2);
	return name;
}
