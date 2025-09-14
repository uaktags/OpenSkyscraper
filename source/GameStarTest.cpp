#include "Game.h"
#include "Application.h"
#include <iostream>

/**
 * Minimal harness to exercise Game::computeStarRating and population/item gating.
 * This is not a formal unit test framework, just a simple runner.
 */
int main() {
    OT::Application dummyApp(0, nullptr); // Requires Application to construct Game.
    OT::Game game(dummyApp);

    struct MinimalPrototype : public OT::Item::AbstractPrototype {
        std::string id = "security";
        OT::Item::Item* make(OT::Game* game) override {
            return new OT::Item::Item(game, this);
        }
    };

    // Baseline: no population
    game.setPopulation(0);
    std::cout << "Population=0 => Stars=" << game.computeStarRating() << std::endl;

    // Just under 300 population => still 1 star
    game.setPopulation(299);
    std::cout << "Population=299 => Stars=" << game.computeStarRating() << std::endl;

    // Hitting 300 =>
    game.setPopulation(300);
    std::cout << "Population=300 => Stars=" << game.computeStarRating() << std::endl;

    // At 1000 but no security =>
    game.setPopulation(1000);
    std::cout << "Population=1000 => Stars=" << game.computeStarRating() << std::endl;

    // Fake-add one security to itemsByType
    MinimalPrototype* proto = new MinimalPrototype();
    OT::Item::Item* dummyItem = new OT::Item::Item(&game, proto);
    game.itemsByType["security"].insert(dummyItem);

    game.setPopulation(1000);
    std::cout << "Population=1000 + Security => Stars=" << game.computeStarRating() << std::endl;

    // Clean up
    delete dummyItem->prototype;
    delete dummyItem;

    return 0;
}