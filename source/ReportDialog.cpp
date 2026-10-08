/* Copyright (c) 2026 OpenSkyscraper contributors */
/* Wave U row 49 - quarterly financial report dialog. */
#include "ReportDialog.h"

#include "Application.h"
#include "Game.h"
#include "Money.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <sstream>

using namespace OT;

namespace {
	/// Display label for each money-ledger category the code actually
	/// produces via Game::transferFunds. The original report instead
	/// reconciles resource 0x3EA per item - elevators billed per car
	/// (std/express/service), escalator per span, lobby per cell
	/// (star-gated), metro, housekeeping, security, recycling and the
	/// parking ramp - but those all land in the single "maintenance"
	/// ledger entry here (Game's quarterlyMaintenanceCost), so the report
	/// shows the ledger categories as they exist. "deposit_refund" and
	/// "metro_fare" are cataloged by FinanceWindow but nothing produces
	/// them yet; they are listed here so a future producer shows up
	/// labelled instead of raw.
	const struct { const char * category; const char * label; } kKnownRows[] = {
		{"rent_income",          "Rent (offices & shops)"},
		{"deposit_income",       "Office move-in deposits"},
		{"deposit_refund",       "Office deposit refunds"},
		{"condo_sale",           "Condo sales"},
		{"condo_buyback",        "Condo buybacks"},
		{"hotel_income",         "Hotel checkouts"},
		{"retail_income",        "Retail (fast food & restaurant)"},
		{"entertainment_income", "Entertainment (cinema & party hall)"},
		{"metro_fare",           "Metro fares"},
		{"treasure",             "Buried treasure"},
		{"easter_egg",           "Windfall"},
		{"maintenance",          "Maintenance"},
		{"construction",         "Construction"},
		{"emergency",            "Emergency (fire crew & ransom)"},
		{"misc",                 "Miscellaneous"},
	};

	bool knownCategory(const std::string & category)
	{
		for (size_t i = 0; i < sizeof(kKnownRows) / sizeof(kKnownRows[0]); i++)
			if (category == kKnownRows[i].category) return true;
		return false;
	}
}

ReportDialog::ReportDialog(Game * game)
	: GameObject(game)
{
	window       = NULL;
	content      = NULL;
	closeButton  = NULL;
	desiredVisible = false; // hidden by default; opens on settlement or 'P'
}

void ReportDialog::close()
{
	if (window)
	{
		app->gui.remove(window);
		window.reset();
		content.reset();
		closeButton.reset();
	}
}

void ReportDialog::reload()
{
	close();
	if (!desiredVisible) return;

	buildWindow();
	populate();
}

void ReportDialog::setVisible(bool visible)
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

void ReportDialog::buildWindow()
{
	window = tgui::ChildWindow::create();
	window->setTitle("Financial Report");
	auto renderer = window->getRenderer();
	renderer->setTitleBarHeight(16 * app->uiScale);
	renderer->setBackgroundColor(sf::Color(35, 40, 55, 240));
	renderer->setTitleBarColor(sf::Color(70, 110, 170));
	renderer->setBorderColor(sf::Color(120, 150, 200));
	renderer->setBorders(2);

	window->setClientSize({320 * app->uiScale, 390 * app->uiScale});

	// Centre on the viewport (same placement as the level-up popup).
	auto sz = app->window.getSize();
	window->setPosition({static_cast<float>(sz.x) / 2.f - 160 * app->uiScale,
	                     static_cast<float>(sz.y) / 2.f - 195 * app->uiScale});

	content = tgui::Label::create();
	content->setTextSize(11 * app->uiScale);
	content->setPosition({10 * app->uiScale, 8 * app->uiScale});
	content->getRenderer()->setTextColor(sf::Color(220, 226, 234));
	content->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Automatic);
	content->setSize({300 * app->uiScale, 336 * app->uiScale});
	window->add(content);

	closeButton = tgui::Button::create("Close");
	closeButton->setSize({80 * app->uiScale, 22 * app->uiScale});
	closeButton->setPosition({120 * app->uiScale, 354 * app->uiScale});
	closeButton->setTextSize(12 * app->uiScale);
	closeButton->onPress([this] { setVisible(false); });
	window->add(closeButton);

	// Tear down when the user clicks the title-bar close widget.
	window->onClose([this] { setVisible(false); });

	app->gui.add(window);
}

void ReportDialog::populate()
{
	if (!window || !content) return;

	const std::map<std::string, int> & ledger = game->money.quarterTotalsByCategory;

	std::ostringstream s;
	char buf[80];
	snprintf(buf, sizeof(buf), "Year %i, Quarter %i", game->time.year, game->time.quarter);
	s << buf << "\n\n";

	// Income rows. The original lists rent per tenant class from the
	// 0x3E9 tables; the ledger aggregates those settlements under
	// "rent_income", so the row shows the quarter's rent total.
	s << "Income:\n";
	std::set<std::string> listed;
	for (size_t i = 0; i < sizeof(kKnownRows) / sizeof(kKnownRows[0]); i++)
	{
		std::map<std::string, int>::const_iterator it = ledger.find(kKnownRows[i].category);
		if (it == ledger.end() || it->second <= 0) continue;
		// Hotel income shows from the checkout ledger even when the
		// hotel population is zero - the original quirk: checkouts
		// record "hotel_income" (Item/Hotel.cpp) regardless of who is
		// currently in a room, and the row is gated on that ledger
		// entry, never on live population.
		s << "  " << kKnownRows[i].label << ":  " << formatMoney(it->second) << "\n";
		listed.insert(kKnownRows[i].category);
	}
	for (std::map<std::string, int>::const_iterator it = ledger.begin();
	     it != ledger.end(); ++it)
	{
		if (it->second <= 0 || listed.count(it->first)) continue;
		s << "  " << it->first << ":  " << formatMoney(it->second) << "\n";
	}
	s << "  Total income:  " << formatMoney(game->money.quarterIncome) << "\n\n";

	// Expense rows (ledger amounts are stored negative).
	s << "Expenses:\n";
	for (size_t i = 0; i < sizeof(kKnownRows) / sizeof(kKnownRows[0]); i++)
	{
		std::map<std::string, int>::const_iterator it = ledger.find(kKnownRows[i].category);
		if (it == ledger.end() || it->second >= 0) continue;
		s << "  " << kKnownRows[i].label << ":  " << formatMoney(-(it->second)) << "\n";
		listed.insert(kKnownRows[i].category);
	}
	for (std::map<std::string, int>::const_iterator it = ledger.begin();
	     it != ledger.end(); ++it)
	{
		if (it->second >= 0 || listed.count(it->first)) continue;
		s << "  " << it->first << ":  " << formatMoney(-(it->second)) << "\n";
	}
	s << "  Total expenses:  " << formatMoney(game->money.quarterExpenses) << "\n\n";

	s << "Balance at quarter start:  " << formatMoney(game->money.quarterStartBalance) << "\n";
	s << "Net:  " << formatMoney(game->money.quarterNet()) << "\n";
	s << "Current balance:  " << formatMoney(game->money.balance) << "\n";

	content->setText(s.str());
}

std::string ReportDialog::formatMoney(int amount) const
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
