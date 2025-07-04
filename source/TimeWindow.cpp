#include "TimeWindow.h"
#include "GameObject.h"
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
#include <string>
#include "Application.h"
#include "Logger.h"
#include "OpenGL.h"
#include "cmath"
#include "Game.h"
#include "TGUI/Widgets/Scrollbar.hpp"

using namespace OT;

TimeWindow::TimeWindow(Game * game) : GameObject(game) {
    window = tgui::ChildWindow::create();
    window->getRenderer()->setTitleBarHeight(10 * app->uiScale);
    
    window->setClientSize({431 * app->uiScale, 41 * app->uiScale});
    window->setPosition({200 * app->uiScale, 22 * app->uiScale});

    rating_rect = tgui::UIntRect(0,0,108,22);
    messageTimer = 0;

    reload();
    
    app->gui.add(window);
}

void TimeWindow::close() {
    window->removeAllWidgets();
}

void TimeWindow::reload() {
    window->removeAllWidgets();

    // BG
    tgui::Texture image = tgui::Texture();
    image.load(app->bitmaps["simtower/ui/time/bg"]);
    auto picture = tgui::Picture::create(image);
    picture->setSize("100%", "100%");
    window->add(picture);

    // Funds
    lblFunds = tgui::Label::create();
    lblFunds->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
    lblFunds->setTextSize(13 * app->uiScale);
    lblFunds->setSize(80 * app->uiScale, 13 * app->uiScale);
    lblFunds->setPosition(345 * app->uiScale, 5 * app->uiScale);
    lblFunds->setScrollbarPolicy(tgui::Scrollbar::Policy::Never);
    window->add(lblFunds);

    // Pops
    lblPopulation = tgui::Label::create();
    lblPopulation->setHorizontalAlignment(tgui::HorizontalAlignment::Right);
    lblPopulation->setTextSize(13 * app->uiScale);
    lblPopulation->setSize(80 * app->uiScale, 13 * app->uiScale);
    lblPopulation->setPosition(345 * app->uiScale, 22 * app->uiScale);
    lblPopulation->setScrollbarPolicy(tgui::Scrollbar::Policy::Never);
    window->add(lblPopulation);

    // message
    lblTooltip = tgui::Label::create();
    lblTooltip->setPosition(42 * app->uiScale, 25 * app->uiScale);
    lblTooltip->setTextSize(11 * app->uiScale);
    window->add(lblTooltip);

    // date, Time
    lblDate = tgui::Label::create();
    lblDate->setPosition(160 * app->uiScale, 3 * app->uiScale);
    lblDate->setTextSize(13 * app->uiScale);
    window->add(lblDate);

    // star rating (scaled)
    // Extract and scale the correct region from the rating spritesheet
    int ratingSrcW = 108, ratingSrcH = 22;
    int scaledW = ratingSrcW * app->uiScale;
    int scaledH = ratingSrcH * app->uiScale;
    sf::Image ratingSheet = app->bitmaps["simtower/ui/time/rating"].copyToImage();
    sf::Image ratingSub;
    ratingSub.create(ratingSrcW, ratingSrcH);
    ratingSub.copy(ratingSheet, 0, 0, sf::IntRect(0, 0, ratingSrcW, ratingSrcH), false);
    sf::Image ratingScaled;
    ratingScaled.create(scaledW, scaledH);
    for (int y = 0; y < scaledH; ++y) {
        for (int x = 0; x < scaledW; ++x) {
            int srcX = x * ratingSrcW / scaledW;
            int srcY = y * ratingSrcH / scaledH;
            ratingScaled.setPixel(x, y, ratingSub.getPixel(srcX, srcY));
        }
    }
    tgui::Texture rating_tx;
    rating_tx.loadFromPixelData({scaledW, scaledH}, ratingScaled.getPixelsPtr());
    rating = tgui::Picture::create(rating_tx);
    rating->setPosition(42 * app->uiScale, 2 * app->uiScale);
    rating->setSize(scaledW, scaledH);
    window->add(rating);

    // watch canvas
    watch = tgui::CanvasSFML::create({29 * app->uiScale, 29 * app->uiScale});
    watch->setPosition(5 * app->uiScale, 5 * app->uiScale);
    window->add(watch);

    updateRating();
    updateFunds();
    updatePopulation();
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
}

void TimeWindow::updateFunds() {
    lblFunds->setText(formatMoney(game->funds));
}

void TimeWindow::updatePopulation() {
    lblPopulation->setText(std::to_string(game->population));
}

void TimeWindow::advance(double dt) {
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

void TimeWindow::renderWatch() {
    sf::Vector2f mid;
    mid.x = watch->getSize().x / 2;
    mid.y = watch->getSize().y / 2;

	double radius_m = watch->getSize().y / 2;
	double radius_h = 0.6 * radius_m;

	//double time    = game->time.getHour();
	double time    = game->time.getHour();

	double angle_h = 2 * M_PI * time/12;
	double angle_m = 2 * M_PI * time;

    //LOG(IMPORTANT, "%f, %f, %f, %f", angle_h, angle_m, game->time.getHour()/12, game->time.getHour());
    watch->clear(tgui::Color::Transparent);
    
    sf::VertexArray lines(sf::Lines, 4);
    lines[0].position = sf::Vector2f(mid.x, mid.y);
    lines[1].position = sf::Vector2f(mid.x + radius_h * sin(angle_h), mid.x + radius_h * -cos(angle_h));
    lines[2].position = sf::Vector2f(mid.x, mid.y);
    lines[3].position = sf::Vector2f(mid.x + radius_m * sin(angle_m), mid.x + radius_m * -cos(angle_m));
    lines[0].color = sf::Color::Black;
    lines[1].color = sf::Color::Black;
    lines[2].color = sf::Color::Black;
    lines[3].color = sf::Color::Black;

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