#include "Application.h"
#include "FinanceWindow.h"
#include "Game.h"
#include "Item/Item.h"
#include "Money.h"

#include <algorithm>
#include <cstdio>
#include <sstream>
#include <vector>

using namespace OT;

namespace {
	/// Display name for each reportable prototype id. Types not in this map
	/// are treated as infrastructure (floor, lobby, stairs, elevators) and
	/// omitted from the per-tenant breakdown.
	const std::map<std::string, std::string> kDisplayNames = {
		{"office",          "Office"},
		{"condo",           "Condo"},
		{"yoot_condo",      "Yoot Condo"},
		{"fastfood",        "Fast Food"},
		{"restaurant",      "Restaurant"},
		{"cinema",          "Movie Theatre"},
		{"partyhall",       "Party Hall"},
		{"hotel_single",    "Single Hotel"},
		{"hotel_double",    "Double Hotel"},
		{"hotel_suite",     "Hotel Suite"},
		{"metro",           "Metro Station"},
		{"parking",         "Parking"},
		{"security",        "Security"},
		{"recycling",       "Recycling"},
		{"medicalcenter",   "Medical Center"},
	};

	/// Maps a money category to the item-type ids that can produce it.
	/// Exclusive categories (single type) get 100% attribution; shared
	/// categories are split between the listed types proportionally by
	/// population.
	const std::map<std::string, std::vector<std::string>> kCategoryTypes = {
		{"rent_income",          {"office"}},
		{"deposit_income",       {"office"}},
		{"deposit_refund",       {"office"}},
		{"condo_sale",           {"condo", "yoot_condo"}},
		{"condo_buyback",        {"condo", "yoot_condo"}},
		{"retail_income",        {"fastfood", "restaurant"}},
		{"entertainment_income", {"cinema", "partyhall"}},
		{"metro_fare",           {"metro"}},
	};
}

FinanceWindow::FinanceWindow(Game * game)
	: GameObject(game)
{
	window = NULL;
	content = NULL;
	desiredVisible = false; // hidden by default; toggle with 'F'
}

void FinanceWindow::close()
{
	if (window)
	{
		app->gui.remove(window);
		window.reset();
		content.reset();
	}
}

void FinanceWindow::reload()
{
	close();
	if (!desiredVisible) return;

	window = tgui::ChildWindow::create();
	window->setTitle("Finance");
	auto renderer = window->getRenderer();
	renderer->setTitleBarHeight(14 * app->uiScale);
	renderer->setBackgroundColor(sf::Color(15, 20, 28, 238));
	renderer->setTitleBarColor(sf::Color(45, 50, 60));
	renderer->setBorderColor(sf::Color(90, 100, 120));
	renderer->setBorders(1);

	window->setClientSize({250 * app->uiScale, 380 * app->uiScale});

	// Anchor below the toolbox on the left edge so it doesn't fight the
	// minimap (right edge) or the time strip (top centre).
	window->setPosition({2.f,
	                     static_cast<float>(280 * app->uiScale)});

	content = tgui::Label::create();
	content->setTextSize(11 * app->uiScale);
	content->setPosition({6 * app->uiScale, 4 * app->uiScale});
	content->getRenderer()->setTextColor(sf::Color(220, 226, 234));
	content->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Automatic);
	content->setSize({238 * app->uiScale, 372 * app->uiScale});
	window->add(content);

	window->onClose([this] { setVisible(false); });

	app->gui.add(window);

	refresh();
}

void FinanceWindow::setVisible(bool visible)
{
	desiredVisible = visible;
	if (visible == isVisible())
	{
		if (visible && !window) reload();
		return;
	}
	if (visible)
	{
		if (!window) reload();
		else window->setVisible(true);
	}
	else
	{
		if (window) window->setVisible(false);
	}
}

std::string FinanceWindow::formatMoney(int amount) const
{
	char c[32];
	bool neg = amount < 0;
	int absAmt = std::abs(amount);
	snprintf(c, 32, "$%i", absAmt);
	std::string fmt(c);
	for (int i = (int)fmt.length() - 3; i > 1; i -= 3)
		fmt.insert(i, ",");
	if (neg) fmt.insert(0, "-");
	return fmt;
}

int FinanceWindow::incomeForType(const std::string & protoId, int population,
                                 const std::map<std::string, int> & sharedPop) const
{
	int total = 0;
	const std::map<std::string, int> & cats = game->money.quarterTotalsByCategory;

	for (std::map<std::string, std::vector<std::string>>::const_iterator it = kCategoryTypes.begin();
	     it != kCategoryTypes.end(); ++it)
	{
		const std::string & cat = it->first;
		const std::vector<std::string> & types = it->second;

		// Does this category apply to the type we're asking about?
		bool applies = false;
		for (size_t i = 0; i < types.size(); i++)
			if (types[i] == protoId) { applies = true; break; }
		if (!applies) continue;

		std::map<std::string, int>::const_iterator cit = cats.find(cat);
		if (cit == cats.end()) continue;
		int catTotal = cit->second;

		if (types.size() == 1)
		{
			total += catTotal;
		}
		else
		{
			// Shared category: split by population. If no type has
			// population, fall back to an even split.
			int totalPop = 0;
			for (size_t i = 0; i < types.size(); i++)
			{
				std::map<std::string, int>::const_iterator sp = sharedPop.find(types[i]);
				if (sp != sharedPop.end()) totalPop += sp->second;
			}
			if (totalPop > 0)
				total += (int)((double)catTotal * population / totalPop);
			else
				total += catTotal / (int)types.size();
		}
	}
	return total;
}

void FinanceWindow::refresh()
{
	if (!window || !content) return;

	// --- Gather per-type aggregates --------------------------------------
	// count[protoId], population[protoId] across all placed items.
	std::map<std::string, int> count;
	std::map<std::string, int> pop;
	for (Game::ItemSet::iterator it = game->items.begin(); it != game->items.end(); ++it)
	{
		Item::Item * item = *it;
		const std::string & id = item->prototype->id;
		// Skip infrastructure: floor, lobby, stairs, escalator, elevators.
		if (kDisplayNames.find(id) == kDisplayNames.end()) continue;
		count[id]++;
		pop[id] += item->population;
	}

	// --- Headline figures -------------------------------------------------
	const std::map<std::string, int> & qc = game->money.quarterTotalsByCategory;

	int totalIncome = game->money.quarterIncome;

	int totalMaintenance = 0;
	{
		std::map<std::string, int>::const_iterator m = qc.find("maintenance");
		if (m != qc.end()) totalMaintenance = -(m->second); // stored negative
	}

	int constructionCosts = 0;
	{
		std::map<std::string, int>::const_iterator c = qc.find("construction");
		if (c != qc.end()) constructionCosts = -(c->second); // stored negative
	}

	// --- Per-type income & sum -------------------------------------------
	int breakdownIncomeSum = 0;
	std::map<std::string, int> incomeByType;
	for (std::map<std::string, int>::iterator di = count.begin(); di != count.end(); ++di)
	{
		const std::string & id = di->first;
		int inc = incomeForType(id, pop[id], pop);
		incomeByType[id] = inc;
		breakdownIncomeSum += inc;
	}

	int otherIncome = totalIncome - breakdownIncomeSum;
	int netRevenues = totalIncome - totalMaintenance;

	// --- Build the text --------------------------------------------------
	std::ostringstream s;

	// Title: handled by the window title bar (set below), but also echo
	// the period at the top of the content for clarity.
	{
		char buf[64];
		snprintf(buf, sizeof(buf), "Year %i, Quarter %i", game->time.year, game->time.quarter);
		window->setTitle(buf);
	}

	s << "Total Income:      " << formatMoney(totalIncome) << "\n";
	s << "Total Maintenance: " << formatMoney(totalMaintenance) << "\n";
	s << "\n";

	// Breakdown header.
	s << "Tenants:\n";
	bool any = false;
	// Walk the kDisplayNames order for a stable, curated layout.
	for (std::map<std::string, std::string>::const_iterator dn = kDisplayNames.begin();
	     dn != kDisplayNames.end(); ++dn)
	{
		const std::string & id = dn->first;
		std::map<std::string, int>::iterator ci = count.find(id);
		if (ci == count.end() || ci->second == 0) continue;

		int p = pop[id];
		int inc = incomeByType[id];
		s << "  " << dn->second << "\n";
		s << "    pop: " << p << "   income: " << formatMoney(inc) << "\n";
		any = true;
	}
	if (!any)
		s << "  (none)\n";

	s << "\n";
	s << "Net Revenues:       " << formatMoney(netRevenues) << "\n";
	s << "Other Income:       " << formatMoney(otherIncome) << "\n";
	s << "Construction Costs: " << formatMoney(constructionCosts) << "\n";
	s << "Last Quarter:       " << formatMoney(game->money.lastQuarterBalance) << "\n";
	s << "Total Balance:      " << formatMoney(game->money.balance) << "\n";

	content->setText(s.str());
}
