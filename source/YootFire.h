/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once

#include <climits>
#include <functional>
#include <vector>

namespace OT {
	namespace Yoot {
		/// What the fire sees in one cell of one floor (T2FloorInfo::GetTenant).
		enum class FireCell {
			None,       ///< no tenant
			Tenant,     ///< a tenant that can burn
			Fireproof,  ///< a tenant with the flag 0x1000 (never burns)
			Burnt       ///< a tenant already burning or burnt down (status 10000)
		};

		/// The tower as T2FireBurning / T2Fire read and change it.
		class FireGrid
		{
		public:
			virtual ~FireGrid() {}
			virtual FireCell cell(int floor, int x) const = 0;
			/// T2Tenant::BurntDown on the tenant covering (floor, x); lo..hi is its
			/// span of cells on that floor.
			virtual void burn(int floor, int x, int & lo, int & hi) = 0;
		};

		/// The three answers of the fire dialog (T2DLG 8110, T2FireBurning::DialogHook
		/// 0x10031570). A refused answer keeps the dialog open with a message.
		struct FireResponse {
			bool accepted = false;
			int mode = 0;            ///< 1 helicopter, 2 security guards, 3 nothing
			long cost = 0;           ///< dollars paid on accepting
			const char * message = ""; ///< shown when refused (helicopter: when sent)
		};
		inline FireResponse fireResponse(int button, long funds, long helicopterCost, bool securityRoom)
		{
			FireResponse r;
			if (button == 0) {
				if (funds < helicopterCost) { r.message = "Not enough money."; return r; }
				r.accepted = true; r.mode = 1; r.cost = helicopterCost;
				r.message = "Control the helicopter by your mouse, and click to spray water, to put the fire out!";
			} else if (button == 1) {
				if (!securityRoom) { r.message = "You can't put the fire out because there is no security room."; return r; }
				r.accepted = true; r.mode = 2;
			} else {
				r.accepted = true; r.mode = 3;
			}
			return r;
		}

		/// One T2Fire: a 96 x 32 px flame (12 cells) that walks along a floor one pixel
		/// a move in `dir`, burning the tenants its front touches, and climbs by
		/// starting new fires on the floor above.
		struct Blaze {
			int floor = 0;
			int left = 0;      ///< sprite left edge, pixels (8 per cell)
			int dir = 1;       ///< +1 right, -1 left
			int frame = 0;     ///< +0: counts moves
			int stopped = 0;   ///< +4: 0 while the front advances, then counts moves since it stopped
			int ownLo = INT_MIN, ownHi = INT_MIN; ///< span of the tenant this fire last burnt
			int fireman = -1;  ///< moves until the assigned fireman reaches it (-1: none)
		};

		/// T2FireBurning's fire list and its rules (Tokyo.t2p 0x10030d50..0x10032590). A fire
		/// move happens once per mover idle, four a game minute.
		class FireModel
		{
		public:
			static const int kPixelsPerCell = 8;
			static const int kWidthPx = 96;        ///< the FIRE bitmap (OBJMAP 5000)
			static const int kHalfWidth = kWidthPx / 2;
			static const int kWidthCells = kWidthPx / kPixelsPerCell;
			static const int kMovesPerMinute = 4;

			std::vector<Blaze> fires;
			int firemen = 0; ///< firemen waiting to be sent (CallFireman: at most 6)
			/// Moves a fireman needs from the security room to a fire (the people
			/// walk through the tower in the original; this is a constant).
			std::function<int(const Blaze &)> travelMoves;

			/// IsBreakoutFire 0x10032000: up to 100 random cells (x in [0, width), row in
			/// [0, rows - 2)); the cell must hold a burnable tenant and the 14 cells
			/// around it must all be tenants. `rnd` is rand(); `floorOf(row)` maps the row to a floor and `xOrigin` the world unit to x.
			bool breakout(const FireGrid & grid, int width, int rows, const std::function<int()> & rnd,
			              const std::function<int(int)> & floorOf, int xOrigin, int & floor, int & x) const;

			/// MakeFire 0x10031b70 (with AdjustFirePos 0x10031cb0): two fires, one each way,
			/// on a 12-cell run of tenants around x; every tenant in the run burns down.
			bool makeFire(FireGrid & grid, int x, int floor);

			/// AdjustFirePos: the 12-cell run [lo, lo + 12) around x, shifted to sit on tenants.
			bool adjust(const FireGrid & grid, int floor, int x, int & lo) const;

			/// IdleEvent 0x100316a0 (one move of every fire, new fires included). With
			/// `guards` the fires are put out by the firemen (ExtinguishByFireman). Returns
			/// the number of fires that burnt out or were put out.
			int idle(FireGrid & grid, bool guards);

			/// T2Fire::Move 0x10032320; true when the fire is gone.
			bool move(FireGrid & grid, size_t index);

			/// DoExtinguish 0x10031e00: the helicopter's water at pixel (px, floor); the first
			/// fire whose box holds it goes out. True when one did.
			bool extinguishAt(int px, int floor);

			static int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }
		};
	}
}
