#pragma once

#include <set>
#include <SFML/Graphics.hpp>
#include <tinyxml2.h>

#include "../GameObject.h"
#include "../Math/Rect.h"
#include "../Person.h"
#include "../Route.h"
#include "../TenantRules.h"
#include "../Sprite.h"
#include "../Path.h"
#include "Prototype.h"

namespace OT {
	namespace Item {

		class Item : public GameObject, public sf::Drawable
		{
		public:
			/// Optional per-instance display name overriding
			/// prototype->name (e.g. authentic tenant name variants,
			/// OpenSkyWeb ISSUE-035). Empty = use the prototype name.
			std::string displayName;
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

			/// Render the occupant AnimPeple sprites with the web-port
			/// pacing and suppression rules (multi-second activity cycles,
			/// per-persona condo frames, waitstaff caps, sleeping filters).
			void renderPeople(sf::RenderTarget & target, const sf::Color & statusTint, bool tinted) const;
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
		/// Number of EXE "visits" (ft % period == phase) crossed by the last
		/// clock advance, capped at 8. Venues and tenants roll their per-visit
		/// dice that many times per tick (batch 5 T26/T32/T36).
		static int visitsThisTick(Game * game, int period = 16, int phase = 0);
		virtual void evacuateForDisaster() {}
		virtual int disasterSaleReversal() const { return 0; }
		bool survivesDisaster() const;

		/// Satisfaction score in [0, 100] maintained by JudgeSystem. Defaults
		/// to 50 (neutral) so unevaluated items remain attractive. Office and
		/// Condo::isAttractive() consult this when deciding move-in/vacation.
		double evaluation;
		virtual double getEvaluation() const { return evaluation; }

		/// EXE tenant record (batch 5 T1): armed/category/tenure/class/redraw
		/// plus six per-member banked-wait counters. The rent class of
		/// office, condo, shop and hotel lives only here.
		struct TenantRecord
		{
			bool    armed = false;
			uint8_t category = 255;  ///< 0 stressed, 1 OK, 2 happy, 255 unjudged (food: 3)
			uint8_t tenure = 0;      ///< 0..120
			uint8_t rentClass = 1;   ///< 0 high .. 3 very low
			bool    redraw = false;
			TenantRules::Bank bank[6];
		};
		TenantRecord tenant;
		/// True for office, condo, hotel rooms, shop, restaurant, fast food.
		bool hasTenantRecord() const;
		/// Change the rent class (clamped 0..3); office/condo/shop rejudge at once.
		virtual void setRentClass(int c);
		/// Derived display score from the tenant category.
		void refreshEvaluation() { evaluation = TenantRules::evaluationFor(tenant.category); }
		void resetTenantBanks();
		/// Reset the record banks and the owner persons' own counters.
		virtual void resetMemberBanks() { resetTenantBanks(); }
		/// CountT category and current contribution subtracted on demolition
		/// (-1 / 0 = none). Overridden by office and condo.
		virtual int populationCategory() const { return -1; }
		virtual int populationContribution() const { return 0; }

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
			bool lobbyRouteComputed = false; ///< false while the lazy route build queue hasn't reached this item
			Route metroRoute;
			virtual void updateRoutes();
			void computeLobbyRoute();

			virtual bool isOccupied() const { return false; }
			virtual bool canHaulPeople() const { return false; }
			virtual bool connectsFloor(int floor) const { return false; }
			virtual bool isStairlike() const { return false; }
			virtual bool isElevator()  const { return false; }
			virtual bool isLobby()     const { return false; }

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
