#pragma once

#include <set>
#include <SFML/Graphics.hpp>
#include <tinyxml2.h>

#include "../GameObject.h"
#include "../Math/Rect.h"
#include "../Person.h"
#include "../Route.h"
#include "../Sprite.h"
#include "../Path.h"
#include "Prototype.h"

namespace OT {
	namespace Item {

		class Item : public GameObject, public sf::Drawable
		{
		public:
			int layer;
			AbstractPrototype * const prototype;
			Item(Game * game, AbstractPrototype * prototype) : GameObject(game), sf::Drawable(), prototype(prototype), size(prototype->size) { layer = 0; population = 0; evaluation = 50; underConstruction = false; constructionEndTime = 0; }
			virtual ~Item();
			virtual void init() {}

			typedef std::set<Sprite *> SpriteSet;
			SpriteSet sprites;
			void addSprite(Sprite * sprite);
			void removeSprite(Sprite * sprite);

			int2 position;
			int2 size;
			void setPosition(int2 p);
			int2 getPosition() const;
			int2 getPositionPixels() const;
			recti getRect() const;
			recti getPixelRect() const;

			void draw(sf::RenderTarget & target, sf::RenderStates states) const;
			virtual void render(sf::RenderTarget & target) const;
			sf::Vector2u getSize() const { return sf::Vector2u(size.x, size.y); }
			sf::Vector2u getSizePixels() const { return sf::Vector2u(size.x*8, size.y*36); }

			virtual rectd getMouseRegion(); //in world pixel

		/// Fine-grained hit test in world-pixel coordinates. Defaults to a
		/// simple rect check against getMouseRegion(); transport items
		/// (stairs, escalators, elevators) override this so clicks pass
		/// through the empty parts of their tile footprint to the tenant
		/// behind. In SimTower you click directly on the stair sprite or
		/// the elevator's motor gearbox, not the whole bounding box.
		virtual bool containsPoint(const double2 & pt);

			virtual void encodeXML(tinyxml2::XMLPrinter & xml);
			virtual void decodeXML(tinyxml2::XMLElement & xml);

			std::string desc() {
				char c[512];
				snprintf(c, 512, "%s floor %i", prototype->id.c_str(), position.y);
				return c;
			}

		virtual void advance(double dt) {}
		virtual int dailyMaintenanceCost() const { return 0; }

		/// Satisfaction score in [0, 100] maintained by JudgeSystem. Defaults
		/// to 50 (neutral) so unevaluated items remain attractive. Office and
		/// Condo::isAttractive() consult this when deciding move-in/vacation.
		double evaluation;
		virtual double getEvaluation() const { return evaluation; }

		/// Construction animation state. Set by Factory::make on fresh
		/// placement; cleared by Game::advance once time.absolute reaches
		/// constructionEndTime. While true, the item renders a placeholder
		/// overlay instead of its real sprites, does not advance, and does
		/// not contribute to daily maintenance. See Phase 4.4.
		bool   underConstruction;
		double constructionEndTime;
		/// Default construction duration for an item of this prototype's
		/// size, in absolute-time units. Subclasses may override.
		virtual double constructionDuration() const;

		typedef std::set<Person *> People;
			People people;
			virtual void addPerson(Person * p);
			virtual void removePerson(Person * p);

			int population;

			Route lobbyRoute;
			Route metroRoute;
			virtual void updateRoutes();

			virtual bool isOccupied() const { return false; }
			virtual bool canHaulPeople() const { return false; }
			virtual bool connectsFloor(int floor) const { return false; }
			virtual bool isStairlike() const { return false; }
			virtual bool isElevator()  const { return false; }

			virtual Path getRandomBackgroundSoundPath() { return ""; }
		};
	}
}

#define OT_ITEM_PROTOTYPE(cls)\
	static AbstractPrototype * makePrototype() {\
		AbstractPrototype * p = new Prototype<cls>;\
		p->entrance_offset = 0;\
		p->exit_offset = 0;\
		initPrototype(p);\
		return p;\
	}\
	static void initPrototype(AbstractPrototype * p)

#define OT_ITEM_CONSTRUCTOR(cls) cls(Game * game, AbstractPrototype * prototype) : Item(game, prototype) {}

#include "../Application.h"
