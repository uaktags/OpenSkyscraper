/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// YootTower plugin-backed tenants: shops, restaurants (incl. fast food and
/// the food court), vending machines and the Spa.
///
/// Every tenant plugin of a covered behaviour family becomes one OpenSky
/// prototype ("yt_<stem>") whose identity, size, price, unlock and art come
/// from the plugin. The family behaviour is the plugin's own
/// ChangeStatusOnIdle / EnterTenant / LeaveTenant code, ported here as a
/// pure state machine (Yoot::TenantModel) so it can be unit-tested; the
/// Item classes (Item/YootTenant.*) only feed it the clock and draw.
///
/// Decompiled sources (Ghidra, ~/yoottower-decomp/{plugins,tenants}-agent):
///   Shop        Barber.t2p FUN_1000bc20 / 1000bfa0 / 1000c0a0 (21 identical
///               shops; Aloha/BCDMart/DutyFree/Fruits/SurfShop and Orange
///               Julius only drop LoadSound)
///   Restaurant  Burger.t2p FUN_1000bc20 / 1000c0b0 / 1000c1e0 (21 restaurants;
///               French drops LoadSound): the shop machine plus
///               PushOutAllCustomer at closing and status 15 (staff carry the
///               garbage out before going home)
///   Vending     Drink.t2p FUN_1000b930 (Drink, KDrink, Cigar, kCigar, Coin
///               byte-identical): no staff, pattern 1 while open
///   (Spa.t2p runs the shop machine and pays without the message.)
/// T2DLL helpers: T2Tenant::CollectCustomer 0x1005f440 (+FromPool/Tenant/
/// Floor), T2CustomerTableIterator 0x1001c790.., T2TenantMemberDef::Initialize
/// 0x10063000 (rate = percent x customerUnits x 0.01),
/// T2TenantDef::CalcNumOfCustomerUnit 0x10062d70, UT2Utils::Float2Int
/// 0x100749f0, T2DateTime::WithinMinutes 0x1001cb90, T2Tenant::CalcInMoney
/// 0x1005f620 / CalcProfitEstimate 0x10061410.
///
/// Pure module: no Game dependency, linked into OpenSkyTests.
#pragma once

#include "PluginDefs.h"
#include "YootPool.h"
#include "YootRules.h"

#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

namespace OT
{
	class AbstractPlugin;
	class PluginManager;

	enum class YootTenantFamily { None, Shop, Restaurant, Vending };

	/// One covered tenant plugin and the OpenSky prototype id it becomes.
	struct YootTenantPlugin
	{
		const char * plugin;       ///< "Icecream.t2p"
		const char * prototypeId;  ///< "yt_icecream"
		YootTenantFamily family;
	};

	/// Every tenant plugin this module covers (56 of the 93 TnPl plugins;
	/// GatheringArea.t2p is a venue item, Item/YootVenueItems.h).
	const std::vector<YootTenantPlugin> & yootTenantPlugins();
	/// Covered entry for a plugin file or prototype id; null when not covered.
	const YootTenantPlugin * findYootTenantPlugin(const std::string & pluginOrPrototypeId);
	bool isYootTenantPrototypeId(const std::string & prototypeId);
	const char * yootTenantFamilyName(YootTenantFamily family);

	/// Everything the family behaviour and the toolbox read from a plugin.
	struct YootTenantDef
	{
		std::string plugin;
		std::string prototypeId;
		YootTenantFamily family = YootTenantFamily::None;

		// ATTR
		uint32_t pluginId = 0;       ///< mID (LE u32 == Mac constant)
		uint32_t worldMask = 0;      ///< mAttrib
		int gameLevel = 1;           ///< mGameLevel (1-based)
		std::string pluginName;      ///< mPluginName

		// TNDF ToolDef / EquipDef / TenantDef
		std::string name;            ///< variation 0 name ("Ice Cream")
		int width = 1, height = 1;   ///< units / floors
		uint32_t attribute = 0;
		int category = 0;            ///< toolbox group (80 Shops, 50 Restaurants)
		std::string categoryName;
		int toolNo = 0;              ///< order inside the group
		int price = 0;               ///< dollars
		int outMoney = 0;            ///< dollars
		int capacity = 0;            ///< T2EquipDef +0x11c
		std::vector<int> inMoney;    ///< dollars per price index
		int numOfKind = 1, numOfPattern = 1;
		int belongCapacity = 0;      ///< staff
		int openMinute = 0, closeMinute = 0, bindMinutes = 0;

		// TMDF id 2 (customers) with its DPDF / PLDF
		bool hasCustomers = false;
		MemberTableDef customers;
		bool hasDayParam = false;
		DayParamDef dayParam;
		bool hasPlaceParam = false;
		PlaceParamDef placeParam;
	};

	/// Fills `out` from the plugin's ATTR, TNDF and TMDF 2 (+DPDF/PLDF named
	/// by the table). False when the plugin is not a tenant or is garbled.
	bool loadYootTenantDef(const AbstractPlugin & plugin, const YootTenantPlugin & entry, YootTenantDef & out);
	/// Loads every covered plugin present in the catalog.
	std::vector<YootTenantDef> loadYootTenantDefs(const PluginManager & plugins);

	namespace Yoot
	{
		// Tenants idle every kTenantIdleMinutes (YootRules.h): 32 round-robin
		// slots at 4 dispatches per game minute (T2TenantArray::DispatchIdle).

		/// T2DateTime::WithinMinutes: a <= now < b with wrap at midnight;
		/// a == b (after wrapping) is the whole day.
		bool withinMinutes(int now, int a, int b);
		/// UT2Utils::Float2Int: integer part, +1 when (frac x 10) > roll % 10.
		int float2Int(float value, int roll);
		/// T2TenantDef::CalcNumOfCustomerUnit: capacity x 8 / bindTime when
		/// the stay is longer than 8 minutes, else the capacity.
		float customerUnits(int capacity, int bindMinutes);
		/// T2TenantMemberDef rate (+0x2c): percent x customerUnits x 0.01.
		float memberRate(uint32_t percent, float units);

		/// T2Tenant status values (+0x08) used by the families.
		enum TenantStatus
		{
			kStatusBuilt = 9,        ///< just built
			kStatusHiring = 10,      ///< waiting for its staff (CollectEmployee)
			kStatusNight = 11,       ///< closed, before midnight
			kStatusClosed = 12,      ///< closed (12/13 behave alike)
			kStatusClosed2 = 13,
			kStatusCalling = 14,     ///< staff called, opening when all are in
			kStatusAfterClose = 15,  ///< restaurant: staff carry the garbage out
			kStatusOpen = 30,        ///< open, empty
			kStatusOpen1 = 31,       ///< customers inside
			kStatusOpen2 = 32,       ///< at least half full
		};

		struct TenantModel;
		struct TenantInputs;

		/// The three customer sources of T2Tenant::CollectCustomer 0x1005f440
		/// (CollectCustomerFromPool 0x1005ef70 / FromTenant 0x1005f0c0 /
		/// FromFloor 0x1005f250). Each returns how many people it called
		/// (they are on their way: TenantModel::walking); `free` is the
		/// capacity minus the people inside and on their way.
		struct CustomerSources
		{
			virtual ~CustomerSources() {}
			virtual int fromPool(const YootTenantDef & def, int free, const TenantInputs & in) { (void)def; (void)free; (void)in; return 0; }
			virtual int fromTenant(const YootTenantDef & def, int free, const TenantInputs & in) { (void)def; (void)free; (void)in; return 0; }
			virtual int fromFloor(const YootTenantDef & def, int free, const TenantInputs & in) { (void)def; (void)free; (void)in; return 0; }
			/// T2Tenant::CollectEmployee 0x1005f5a0 (TMDF 1): true when the
			/// staff came out of the pool.
			virtual bool hire(const YootTenantDef & def) { (void)def; return true; }
		};

		/// What the tower offers the tenant at an idle (CollectCustomer and
		/// CollectEmployee inputs, reduced to what OpenSky models).
		struct TenantInputs
		{
			int minute = 0;          ///< minute of day
			long long absMinute = 0; ///< absolute game minute
			bool holiday = false;
			bool rain = false;
			bool reachable = true;   ///< a route from the lobby exists
			/// The customer sources; null = nobody comes.
			CustomerSources * sources = nullptr;
		};

		/// Outcome of one call, for the item (money, art, messages).
		struct TenantEvents
		{
			bool patternChanged = false;
			bool paid = false;
			int income = 0;          ///< dollars (CalcInMoney x 100), may be < 0
			int paidCustomers = 0;   ///< TotalCustomer (+0xa8) at the payment
			bool estimateChanged = false;
			/// CallBelongPeople (with the visitor pool): the staff were sent
			/// for; each one counts in staffIn when it walks in (AddPopulation).
			bool callStaff = false;
		};

		/// Pure state of one tenant (T2Tenant fields the families touch).
		struct TenantModel
		{
			int status = kStatusBuilt;     ///< +0x08
			int pattern = 0;               ///< +0x3c (within kind 0)
			int staff = 0;                 ///< hired staff (belong people)
			int staffIn = 0;               ///< +0x9c staff at work
			int walking = 0;               ///< +0x98 customers called and on their way
			std::vector<long long> inside;   ///< +0xa0 customers inside (leave minute)
			int customers = 0;             ///< +0xa8 customers since opening (CalcInMoney)
			int estimate = 0;              ///< +0x90 1..300 (0 = not yet)

			int insideCount() const { return (int)inside.size(); }
		};

		/// One ChangeStatusOnIdle call for the def's family. `rng` returns
		/// rand()-like values (tests pass a deterministic sequence).
		template <typename Rng>
		void tenantIdle(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, Rng && rng, TenantEvents & ev);

		/// Per-minute bookkeeping: stays that ended leave (LeaveTenant).
		/// Updates the pattern like the family overrides.
		void tenantMinute(TenantModel & m, const YootTenantDef & def, long long absMinute, TenantEvents & ev);
		/// A called customer reaches the tenant (T2Tenant::EnterTenant +
		/// the family EnterTenant / EnterCustomer): false when the tenant is
		/// not open (the person turns back with -100 estimate). The stay
		/// ends at absMinute + bindTime.
		bool customerArrives(TenantModel & m, const YootTenantDef & def, long long absMinute, TenantEvents & ev);
		/// A customer on the way gave up or was removed.
		void customerCancelled(TenantModel & m);

		/// T2Tenant::CollectCustomerFromPool 0x1005ef70 against a pool with
		/// the CheckRouteFromPool mask: the customer table is walked from a
		/// random member (T2CustomerTableIterator), each member asks for
		/// Float2Int(rate) people (at most `free` in total) and stops at the
		/// first failed T2Pool::Call. `onCalled` gets each person's type and
		/// start minute (now + 2 per person of the member). Returns the count.
		int collectFromPool(const YootTenantDef & def, Pool & pool, unsigned routeMask, int free, int nowMinute,
			const PoolRng & rng, const std::function<void(const PeopleType &, int)> & onCalled);
		/// T2TenantMemberTableDef::GetEconoType of the customer table.
		int customerEconoType(const YootTenantDef & def);
		/// GetScore(EPlace): PLDF score, or 1000 for the pool without a PLDF.
		int customerScore(const YootTenantDef & def, int place);

		/// Pattern of the shop/restaurant overrides for `inside` customers:
		/// 0 closed, 1 open and empty, 2 from 1 customer, 3 from half the
		/// capacity (EnterTenant thresholds 1 and capacity/2).
		int commercialPattern(int status, int inside, int capacity);

		/// Income/estimate helpers (CalcInMoney / CalcProfitEstimate).
		int tenantIncome(const TenantModel & m, const YootTenantDef & def);
		int tenantEstimate(const TenantModel & m, const YootTenantDef & def);

		/// Implementation detail of tenantIdle (non-template part).
		struct RngFn { int (*fn)(void *); void * ctx; int operator()() const { return fn(ctx); } };
		void tenantIdleImpl(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, const RngFn & rng, TenantEvents & ev);

		template <typename Rng>
		void tenantIdle(TenantModel & m, const YootTenantDef & def, const TenantInputs & in, Rng && rng, TenantEvents & ev)
		{
			typedef typename std::remove_reference<Rng>::type R;
			RngFn f{[](void * c) { return (*static_cast<R *>(c))(); }, (void *)&rng};
			tenantIdleImpl(m, def, in, f, ev);
		}
	}

	/// Toolbox layout (game-parity S4): groups by ToolDef category, items in
	/// each group by T2ToolDef::GetSortKey (category << 16 | toolNo).
	struct YootToolboxEntry
	{
		std::string prototypeId;
		int category = 0;
		std::string categoryName;
		int toolNo = 0;
	};
	struct YootToolboxGroup
	{
		int category = 0;
		std::string name;
		std::vector<std::string> prototypeIds;
	};
	std::vector<YootToolboxGroup> yootToolboxGroups(std::vector<YootToolboxEntry> entries);
}
