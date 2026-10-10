#pragma once
#include "../Sprite.h"
#include "Item.h"
#include "../YootItemArt.h"
#include <queue>
#include <set>

namespace OT { namespace Item {

class Parking;
class Housekeeping;

class Hotel : public Item {
public:
	OT_ITEM_CONSTRUCTOR(Hotel);
	static AbstractPrototype *makeSinglePrototype();
	static AbstractPrototype *makeDoublePrototype();
	static AbstractPrototype *makeSuitePrototype();
	~Hotel() override;

	enum RoomType { kSingle=0, kDouble=1, kSuite=2 };
	enum RoomState { kClean=0, kOccupied=1, kDirty=2, kInfested=3 };
	// kInfested: cockroach frames (sheet cols 7/8); the hotel
	// lifecycle arms it (wave S) — render support only here.

	int variant;
	int subVariant;
	int yootKind = 0; ///< Skin::Yoot View kind roll (Yoot::roomKindRoll)
	int roomState;
	int hosted = 0;
	double dirtySince;
	bool dinnerSent = false;
	bool evacuated = false;
	int capacity() const;
	void setRentClass(int c);

	class Guest : public Person
	{
	public:
		Guest(Hotel *item, int memberIndex = 0);
		virtual ~Guest() { LOG(DEBUG, "hotel guest %p killed", this); }

		Hotel *hotel;
		// SimTower lifecycle (art branch batches): banked waits, dinner
		// trip and parking flags.
		bool atHotel = false;
		bool leaving = false;
		Parking *parkingUsed = nullptr; ///< freed on checkout
		Item *entry = nullptr;
		// YootTower stay schedule (PeopleSystem books these).
		double arrivalTime;
		double dinnerLeaveTime;
		double dinnerReturnTime;
		double sleepTime;
		double wakeTime;
		double checkoutTime;

		struct laterThan
		{
			bool operator()(const Guest *a, const Guest *b) const
			{
				return (a->arrivalTime > b->arrivalTime);
			}
		};
	};

	class Housekeeper : public Person
	{
	public:
		Housekeeper(Hotel *item);
		virtual ~Housekeeper() { LOG(DEBUG, "hotel housekeeper %p killed", this); }

		double cleaningUntil;
		bool cleaning;
	};

	std::priority_queue<Guest *, std::deque<Guest *>, Guest::laterThan> arrivingGuests;
	std::set<Guest *, SimLess> guests;
	Housekeeper *housekeeper = nullptr;

	void init() override;
	void encodeXML(tinyxml2::XMLPrinter&) override;
	void decodeXML(tinyxml2::XMLElement&) override;
	void advance(double) override;
	virtual int dailyMaintenanceCost() const override { return 600; }
	void addPerson(Person*) override;
	void removePerson(Person*) override;
	void evacuateForDisaster() override;
	int populationCategory() const override { return variant + 1; }
	int populationContribution() const override { return hosted; }

	void stampInfested();
	void checkout(bool paid);
	bool checkIn(int memberIndex = -1);
	static void spreadRoaches(Game*);
	static void judgeAll(Game*);

	/// YootTower (plan step 14): the stay's check-in is booked on the
	/// linked Front Desk once; dirty rooms wait for a Housekeeping maid.
	bool yootStayBooked = false;
	/// A saved stay (yootStayStatus / yootStayParty) read by decodeXML and
	/// restored by PeopleSystem::roomIdle on the first idle tick after load.
	int pendingStayStatus = 0;
	std::string pendingStayParty;
	bool yootSendHousekeeper(Item * from);

	Sprite sprite;
	bool spriteNeedsUpdate;
	// Last frame column updateSprite() selected (detects the
	// day/night and 4AM lights-out re-poses).
	int roomFrameIndex;
	void updateSprite();
	/// Skin::Yoot: the room plugin's View sheet (Singleroom, Twinroom,
	/// Suiteroom; Smallroom/Largeroom in Kegon), null = OpenSky art.
	const YootPluginView * yootView = nullptr;
	/// Frame updateSprite() would pick now (sheet column or View band).
	int currentFrame() const;

	Path getRandomBackgroundSoundPath() override;

protected:
	void sendGuestHome(Guest *g, bool paid);
	void scheduleGuest(Guest *g);
	void clearAll(bool adjustPopulation = true);
	void applyVariant();
	void resetBanks();
};

} }
