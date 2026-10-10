/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootFire.h"

#include <algorithm>

namespace OT {
	namespace Yoot {
		bool FireModel::breakout(const FireGrid & grid, int width, int rows, const std::function<int()> & rnd,
		                         const std::function<int(int)> & floorOf, int xOrigin, int & floor, int & x) const
		{
			for (int i = 0; i < 100; ++i) {
				const int cx = (width > 0 ? rnd() % width : 0) - xOrigin;
				const int row = rows - 2 != 0 ? rnd() % (rows - 2) : 0;
				const int f = floorOf(row);
				if (grid.cell(f, cx) != FireCell::Tenant) continue;
				bool all = true;
				for (int u = cx - kWidthCells / 2 - 1; u < cx + 1 + kWidthCells / 2 && all; ++u)
					all = grid.cell(f, u) != FireCell::None;
				if (!all) continue;
				floor = f;
				x = cx;
				return true;
			}
			return false;
		}

		bool FireModel::adjust(const FireGrid & grid, int floor, int x, int & lo) const
		{
			int l = x - kWidthCells / 2, shift = 0;
			for (int tries = 0; tries <= kWidthCells / 2; ++tries) {
				const bool left = grid.cell(floor, l) != FireCell::None;
				const bool right = grid.cell(floor, l + kWidthCells - 1) != FireCell::None;
				if (!left) {
					if (right) {
						if (shift < 0) return false;
						shift = 1;
						++l;
					}
				} else if (right) {
					for (int u = l; u < l + kWidthCells; ++u)
						if (grid.cell(floor, u) == FireCell::None) return false;
					lo = l;
					return true;
				} else {
					if (shift > 0) return false;
					shift = -1;
					--l;
				}
			}
			return false;
		}

		bool FireModel::makeFire(FireGrid & grid, int x, int floor)
		{
			if (grid.cell(floor, x) != FireCell::Tenant) return false;
			int lo;
			if (!adjust(grid, floor, x, lo)) return false;
			Blaze pair[2];
			for (int k = 0; k < 2; ++k) {
				pair[k].floor = floor;
				pair[k].left = lo * kPixelsPerCell;
				pair[k].dir = k == 0 ? 1 : -1;
			}
			// T2Fire::T2Fire remembers a tenant of its run (the rightmost for the fire going
			// right, the leftmost for the other); BurntDown then burns the whole run, and a fire
			// may walk on through its own tenant.
			bool first = true;
			for (int u = lo; u < lo + kWidthCells; ++u)
				if (grid.cell(floor, u) == FireCell::Tenant) {
					int a, b;
					grid.burn(floor, u, a, b);
					if (first) { pair[1].ownLo = a; pair[1].ownHi = b; first = false; }
					pair[0].ownLo = a; pair[0].ownHi = b;
				}
			fires.push_back(pair[0]);
			fires.push_back(pair[1]);
			return true;
		}

		bool FireModel::move(FireGrid & grid, size_t index)
		{
			Blaze b = fires[index];
			if (b.stopped == 0) {
				const int front = b.dir <= 0 ? b.left - 1 : b.left + kWidthPx + 1;
				const int cx = floorDiv(front, kPixelsPerCell);
				const FireCell kind = grid.cell(b.floor, cx);
				const bool own = cx >= b.ownLo && cx < b.ownHi;
				if (kind == FireCell::None || kind == FireCell::Fireproof || (kind == FireCell::Burnt && !own)) {
					b.stopped = 1;
				} else {
					b.left += b.dir;
					if (kind == FireCell::Tenant) grid.burn(b.floor, cx, b.ownLo, b.ownHi);
				}
			} else {
				++b.stopped;
			}
			bool gone = false;
			if (++b.frame % kHalfWidth == 0) {
				// The fire starts its climb at the floor above, from the edge it came from.
				const int ox = b.dir <= 0 ? floorDiv(b.left + kWidthPx, kPixelsPerCell) : floorDiv(b.left, kPixelsPerCell);
				fires[index] = b; // makeFire appends: keep this fire's state first
				int x = ox;
				for (int k = 0; k < kWidthCells; ++k) {
					if (makeFire(grid, x, b.floor + 1) || b.stopped == 0) break;
					x += b.dir;
				}
				if (kHalfWidth < b.stopped) gone = true;
			}
			fires[index] = b;
			return gone;
		}

		int FireModel::idle(FireGrid & grid, bool guards)
		{
			int removed = 0;
			// The list grows while it is walked (MakeFire appends); the original's
			// index loop moves the new fires in the same pass.
			for (size_t i = 0; i < fires.size();) {
				const bool burntOut = move(grid, i);
				bool out = burntOut;
				if (!burntOut && guards) {
					Blaze & b = fires[i];
					if (b.fireman < 0) {
						if (firemen > 0) {
							--firemen;
							b.fireman = travelMoves ? travelMoves(b) : 0;
						}
					} else if (b.fireman > 0) {
						if (--b.fireman == 0) {
							// The fireman is at the fire: ExtinguishByFireman sets the counter
							// to half - 4 (or 1) so the fire stops spreading and burns out.
							if (b.stopped < kHalfWidth - 4 || b.stopped == 0) b.stopped = kHalfWidth - 4;
							b.fireman = 0;
						}
					}
					if (b.fireman == 0 && b.stopped > kHalfWidth) out = true; // style 2: extinguish once over half
				}
				if (out) {
					if (guards && fires[i].fireman >= 0) ++firemen; // the fireman is free again
					fires.erase(fires.begin() + i);
					++removed;
				} else {
					++i;
				}
			}
			return removed;
		}

		bool FireModel::extinguishAt(int px, int floor)
		{
			for (size_t i = 0; i < fires.size(); ++i) {
				const Blaze & b = fires[i];
				if (b.floor == floor && px >= b.left && px < b.left + kWidthPx) {
					fires.erase(fires.begin() + i);
					return true;
				}
			}
			return false;
		}
	}
}
