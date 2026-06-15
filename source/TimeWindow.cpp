#include "TimeWindow.h"
#include "GameObject.h"
#include "TimeWindowStyle.h"
#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/VertexArray.hpp>
#include <SFML/System/Vector2.hpp>
#include <TGUI/Backend/Renderer/OpenGL3/CanvasOpenGL3.hpp>
#include <TGUI/Layout.hpp>
#include <TGUI/Rect.hpp>
#include <TGUI/Texture.hpp>
#include <TGUI/Widgets/ChildWindow.hpp>
#include <TGUI/Widgets/Label.hpp>
#include <TGUI/Widgets/Picture.hpp>
#include <cstdlib>
#include <sstream>
#include <string>
#include "Application.h"
#include "Logger.h"
#include "OpenGL.h"
#include "cmath"
#include "Game.h"
#include "TGUI/Widgets/Scrollbar.hpp"

using namespace OT;

static sf::Color blendColor(sf::Color a, sf::Color b, float t)
{
    return sf::Color(
        static_cast<uint8_t>(a.r + (b.r - a.r) * t),
        static_cast<uint8_t>(a.g + (b.g - a.g) * t),
        static_cast<uint8_t>(a.b + (b.b - a.b) * t),
        static_cast<uint8_t>(a.a + (b.a - a.a) * t));
}

static void fillRect(sf::Image &image, unsigned int x, unsigned int y, unsigned int w, unsigned int h, sf::Color color)
{
    const unsigned int maxX = std::min(x + w, image.getSize().x);
    const unsigned int maxY = std::min(y + h, image.getSize().y);
    for (unsigned int py = y; py < maxY; py++)
        for (unsigned int px = x; px < maxX; px++)
            image.setPixel({px, py}, color);
}

static sf::Image makeModernHeaderBackground(const TimeWindowLayout &layout)
{
    sf::Image image({static_cast<unsigned int>(layout.clientWidth), static_cast<unsigned int>(layout.clientHeight)}, sf::Color::Transparent);
    const sf::Color top(24, 30, layout.backgroundTopBlue, 238);
    const sf::Color bottom(12, 15, layout.backgroundBottomBlue, 238);
    const sf::Color panel(42, 49, 62, 205);
    const sf::Color line(74, 91, 112, 220);
    const sf::Color accent(0, 170, layout.accentBlue, 235);

    for (unsigned int y = 0; y < image.getSize().y; y++)
    {
        const float t = static_cast<float>(y) / static_cast<float>(image.getSize().y - 1);
        fillRect(image, 0, y, image.getSize().x, 1, blendColor(top, bottom, t));
    }

    fillRect(image, 0, 0, image.getSize().x, 1, sf::Color(126, 209, 255, 190));
    fillRect(image, 0, image.getSize().y - 1, image.getSize().x, 1, sf::Color(4, 8, 14, 230));
    fillRect(image, 0, 0, 1, image.getSize().y, sf::Color(126, 209, 255, 125));
    fillRect(image, image.getSize().x - 1, 0, 1, image.getSize().y, sf::Color(4, 8, 14, 230));
    fillRect(image, 2, 2, 4, image.getSize().y - 4, accent);

    // Common Y coordinates for all three cards: Y = s(6) to s(50) (height s(44))
    const unsigned int cY = static_cast<unsigned int>(layout.clientHeight * 6 / 56);
    const unsigned int cH = static_cast<unsigned int>(layout.clientHeight * 44 / 56);
    const unsigned int midY = layout.tooltipBackgroundY;

    // Helper lambda to draw a card's 1-pixel border
    auto drawCardBorder = [&](unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
        fillRect(image, x, y, w, 1, line);          // Top
        fillRect(image, x, y + h - 1, w, 1, line);  // Bottom
        fillRect(image, x, y, 1, h, line);          // Left
        fillRect(image, x + w - 1, y, 1, h, line);  // Right
    };

    // 3. Draw Watch Card (X = s(8) to s(46))
    const unsigned int watchCardX = layout.watchX - (layout.watchX - 8) * (layout.watchX > 8);
    const unsigned int watchCardW = layout.watchSize + (layout.watchX - watchCardX) * 2;
    fillRect(image, watchCardX, cY, watchCardW, cH, sf::Color(9, 14, 22, 170));
    drawCardBorder(watchCardX, cY, watchCardW, cH);

    // 4. Draw Middle Card (X = layout.tooltipBackgroundX to layout.tooltipBackgroundWidth)
    // Top Half: Star rating and Date/Time (panel background)
    fillRect(image, layout.tooltipBackgroundX, cY, layout.tooltipBackgroundWidth, midY - cY, panel);
    // Bottom Half: Tooltip and Ledger (light-blue background)
    fillRect(image, layout.tooltipBackgroundX, midY, layout.tooltipBackgroundWidth, cY + cH - midY, sf::Color(232, 240, 248, 220));
    // Middle divider separating top and bottom halves
    fillRect(image, layout.tooltipBackgroundX, midY, layout.tooltipBackgroundWidth, 1, line);
    // Outer border
    drawCardBorder(layout.tooltipBackgroundX, cY, layout.tooltipBackgroundWidth, cH);

    // 5. Draw Metrics Card (X = layout.tooltipBackgroundX + layout.tooltipBackgroundWidth to clientWidth - s(8))
    const unsigned int metricsCardX = layout.tooltipBackgroundX + layout.tooltipBackgroundWidth;
    const unsigned int metricsCardW = layout.clientWidth - metricsCardX - (layout.clientWidth - 512) * (layout.clientWidth > 512);
    fillRect(image, metricsCardX, cY, metricsCardW, cH, panel);
    // Horizontal divider inside metrics card (aligned with the middle card's divider)
    fillRect(image, metricsCardX, midY, metricsCardW, 1, sf::Color(74, 91, 112, 125));
    // Outer border
    drawCardBorder(metricsCardX, cY, metricsCardW, cH);

    return image;
}

static void setLabelColor(tgui::Label::Ptr label, sf::Color color)
{
    label->getRenderer()->setTextColor(color);
}

TimeWindow::TimeWindow(Game * game) : GameObject(game) {
    const TimeWindowLayout layout = makeModernTimeWindowLayout(app->uiScale);
    window = tgui::ChildWindow::create();
    auto renderer = window->getRenderer();
    renderer->setTitleBarHeight(0);
    renderer->setBackgroundColor(sf::Color::Transparent);
    renderer->setBorderColor(sf::Color::Transparent);
    renderer->setBorders(0);
    
    window->setClientSize({static_cast<float>(layout.clientWidth), static_cast<float>(layout.clientHeight)});
    window->setPosition({128 * app->uiScale, 22 * app->uiScale});

    rating_rect = tgui::UIntRect(0,0,108,22);
    messageTimer = 0;

    app->gui.add(window);
}

void TimeWindow::close() {
    if (window) {
        window->removeAllWidgets();
        app->gui.remove(window);
    }
}

void TimeWindow::reload() {
    const TimeWindowLayout layout = makeModernTimeWindowLayout(app->uiScale);
    window->removeAllWidgets();

    // BG
    sf::Image backgroundImage = makeModernHeaderBackground(layout);
    tgui::Texture image;
    image.loadFromPixelData(backgroundImage.getSize(), backgroundImage.getPixelsPtr());
    auto picture = tgui::Picture::create(image);
    picture->setSize("100%", "100%");
    window->add(picture);

    lblFundsTitle = tgui::Label::create("FUND");
    lblFundsTitle->setTextSize(10 * app->uiScale);
    lblFundsTitle->setPosition(layout.metricsX, layout.metricsY);
    lblFundsTitle->setSize(42 * app->uiScale, 13 * app->uiScale);
    lblFundsTitle->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblFundsTitle, sf::Color(131, 210, 255));
    window->add(lblFundsTitle);

    lblPopulationTitle = tgui::Label::create("POP");
    lblPopulationTitle->setTextSize(10 * app->uiScale);
    lblPopulationTitle->setPosition(layout.metricsX, layout.metricsY + layout.metricRowHeight);
    lblPopulationTitle->setSize(42 * app->uiScale, 13 * app->uiScale);
    lblPopulationTitle->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblPopulationTitle, sf::Color(131, 210, 255));
    window->add(lblPopulationTitle);

    // Funds
    lblFunds = tgui::Label::create();
    lblFunds->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
    lblFunds->setTextSize(12 * app->uiScale);
    lblFunds->setSize(layout.metricsWidth - 44 * app->uiScale, 14 * app->uiScale);
    lblFunds->setPosition(layout.metricsX + 44 * app->uiScale, layout.metricsY);
    lblFunds->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblFunds, sf::Color(244, 248, 252));
    window->add(lblFunds);

    // Pops
    lblPopulation = tgui::Label::create();
    lblPopulation->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
    lblPopulation->setTextSize(12 * app->uiScale);
    lblPopulation->setSize(layout.metricsWidth - 44 * app->uiScale, 14 * app->uiScale);
    lblPopulation->setPosition(layout.metricsX + 44 * app->uiScale, layout.metricsY + layout.metricRowHeight);
    lblPopulation->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblPopulation, sf::Color(244, 248, 252));
    window->add(lblPopulation);

    lblMoneyStats = tgui::Label::create();
    lblMoneyStats->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
    lblMoneyStats->setTextSize(10 * app->uiScale);
    lblMoneyStats->setSize(126 * app->uiScale, 12 * app->uiScale);
    lblMoneyStats->setPosition(layout.metricsX - 140 * app->uiScale, 35 * app->uiScale);
    lblMoneyStats->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblMoneyStats, sf::Color(61, 71, 82));
    window->add(lblMoneyStats);

    // message
    lblTooltip = tgui::Label::create();
    lblTooltip->setPosition(layout.tooltipX, layout.tooltipY);
    lblTooltip->setTextSize(11 * app->uiScale);
    lblTooltip->setSize(layout.tooltipWidth, 14 * app->uiScale);
    lblTooltip->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblTooltip, sf::Color(30, 36, 43));
    window->add(lblTooltip);

    // date, Time
    lblDate = tgui::Label::create();
    lblDate->setHorizontalAlignment(tgui::HorizontalAlignment::Center);
    lblDate->setPosition(layout.dateX, layout.dateY);
    lblDate->setSize(172 * app->uiScale, 22 * app->uiScale);
    lblDate->setTextSize(14 * app->uiScale);
    lblDate->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblDate, sf::Color(229, 238, 246));
    window->add(lblDate);

    lblRating = tgui::Label::create();
    lblRating->setPosition(layout.ratingX, layout.ratingY);
    lblRating->setSize(layout.ratingWidth, layout.ratingHeight + 6 * app->uiScale);
    lblRating->setTextSize(15 * app->uiScale);
    lblRating->getScrollbar()->setPolicy(tgui::Scrollbar::Policy::Never);
    setLabelColor(lblRating, sf::Color(255, 218, 70));
    window->add(lblRating);

    // watch canvas
    watch = tgui::CanvasSFML::create({static_cast<float>(layout.watchSize), static_cast<float>(layout.watchSize)});
    watch->setPosition(layout.watchX, layout.watchY);
    window->add(watch);

    updateRating();
    updateFunds();
    updatePopulation();
    updateMoneyStats();
    updateTime();
}

void TimeWindow::updateTime() {
    Time & t = game->time;
		
	std::string timestring;
	timestring.append(t.day == 0 ? "1st WD" : "2nd WD");
	//weekday->SetProperty("display", (t.day == 2 ? "none" : "inline"));
	//weekend->SetProperty("display", (t.day != 2 ? "none" : "inline"));
	
	timestring.append(" / " + std::to_string(t.quarter) + "Q");

	const char * suffix = "th Yr";
	int yl = (t.year % 10);
	if (yl == 1) suffix = "st Yr";
	if (yl == 2) suffix = "nd Yr";
	if (yl == 3) suffix = "rd Yr";
	timestring.append(" / " + std::to_string(t.year) + suffix);

    lblDate->setText(timestring);    
}

void TimeWindow::updateTooltip() {
    std::stringstream str;
    displayedSpeedMode = game->speedMode;
	if (game->toolPrototype) {
		str << "Construct ";
		str << game->toolPrototype->name;
		str << " ";
		str << formatMoney(game->toolPrototype->price);
	} else {
		if (game->selectedTool == "bulldozer") str << "Bulldoze";
		if (game->selectedTool == "finger") str << "Resize elevator shaft";
		if (game->selectedTool == "inspector") str << "Inspect";
	}
	if (!str.str().empty()) str << "  |  ";
	switch (game->speedMode) {
		case 0: str << "Paused"; break;
		case 1: str << "Speed 1x"; break;
		case 2: str << "Speed 2x"; break;
		case 3: str << "Speed 4x"; break;
		default: break;
	}
	if (!message.empty()) {
		if (!str.str().empty()) str << "  |  ";
		str << message;
	}
    if (str.str() != "") {
        lblTooltip->setText(str.str());
    }
}

void TimeWindow::showMessage(std::string msg)
{
	LOG(IMPORTANT, msg.c_str());
	message = msg;
	messageTimer = 3;
	updateTooltip();
}

void TimeWindow::updateRating() {
    unsigned int pos = 22 * (game->rating);
    LOG(IMPORTANT, "%i", pos);
    // rating_rect = tgui::UIntRect(0, pos, 108, 22);
    rating_rect.top = pos;
    if (lblRating)
    {
        std::string stars;
        for (int i = 0; i < 5; i++)
            stars += i <= game->rating ? "★" : "☆";
        lblRating->setText(stars);
    }
}

void TimeWindow::updateFunds() {
    lblFunds->setText(formatMoney(game->funds));
}

void TimeWindow::updatePopulation() {
    lblPopulation->setText(std::to_string(game->population));
}

void TimeWindow::updateMoneyStats() {
    if (!lblMoneyStats)
        return;

    std::stringstream str;
    str << "D " << formatSignedCompactMoney(game->money.todayIncome)
        << "/" << formatSignedCompactMoney(-game->money.todayExpenses)
        << " W " << formatSignedCompactMoney(game->money.recentNet());
    lblMoneyStats->setText(str.str());
}

void TimeWindow::advance(double dt) {
    if (displayedSpeedMode != game->speedMode) {
        updateTooltip();
    }

    if (messageTimer > 0) {
		messageTimer -= dt;
		if (messageTimer <= 0) {
			message.clear();
			messageTimer = 0;
			updateTooltip();
		}
	}

    renderWatch();
}

void TimeWindow::setVisible(bool visible) {
    if (window) window->setVisible(visible);
}

bool TimeWindow::isVisible() const {
    return window && window->isVisible();
}

void TimeWindow::renderWatch() {
    sf::Vector2f mid;
    mid.x = watch->getSize().x / 2;
    mid.y = watch->getSize().y / 2;

	double radius_m = watch->getSize().y / 2;
	double radius_h = 0.6 * radius_m;

	//double time    = game->time.getHour();
	double time    = game->time.getHour();

	const double pi = 3.14159265358979323846;
	double angle_h = 2 * pi * time/12;
	double angle_m = 2 * pi * time;

    //LOG(IMPORTANT, "%f, %f, %f, %f", angle_h, angle_m, game->time.getHour()/12, game->time.getHour());
    watch->clear(tgui::Color::Transparent);
    
    sf::VertexArray lines(sf::PrimitiveType::Lines, 4);
    lines[0].position = sf::Vector2f(mid.x, mid.y);
    lines[1].position = sf::Vector2f(mid.x + radius_h * sin(angle_h), mid.x + radius_h * -cos(angle_h));
    lines[2].position = sf::Vector2f(mid.x, mid.y);
    lines[3].position = sf::Vector2f(mid.x + radius_m * sin(angle_m), mid.x + radius_m * -cos(angle_m));
    lines[0].color = sf::Color(230, 240, 248);
    lines[1].color = sf::Color(230, 240, 248);
    lines[2].color = sf::Color(0, 190, 255);
    lines[3].color = sf::Color(0, 190, 255);

    watch->draw(lines);
    watch->display();
}

std::string TimeWindow::formatMoney(int amount)
{
	char c[32];
	snprintf(c, 32, "$%i", amount);
	string fmt(c);
	for (int i = (int)fmt.length() - 3; i > 1; i -= 3) fmt.insert(i, "'");
	return fmt;
}

std::string TimeWindow::formatCompactMoney(int amount)
{
	int absAmount = std::abs(amount);
	char c[32];
	if (absAmount >= 1000000)
		snprintf(c, 32, "$%iM", absAmount / 1000000);
	else if (absAmount >= 1000)
		snprintf(c, 32, "$%ik", absAmount / 1000);
	else
		snprintf(c, 32, "$%i", absAmount);
	return std::string(c);
}

std::string TimeWindow::formatSignedCompactMoney(int amount)
{
	if (amount < 0)
		return "-" + formatCompactMoney(amount);
	return "+" + formatCompactMoney(amount);
}
