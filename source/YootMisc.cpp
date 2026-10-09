/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootMisc.h"

#include "YootEvents.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace OT
{
	namespace Yoot
	{
		// ---------------------------------------------------------- School

		void SchoolModel::enter()
		{
			++inside;
			if (inside == 1) { status = Some; pattern = 1; }
			if (inside == capacity / 2) { status = Half; pattern = 3; }
		}

		void SchoolModel::leave()
		{
			if (inside <= 0) return;
			--inside;
			if (inside == capacity / 2 - 1) { status = Some; pattern = 1; }
		}

		void SchoolModel::pushOut(int n)
		{
			for (int k = 0; k < n && inside > 0; ++k) leave();
		}

		void SchoolModel::arrive(long long now)
		{
			std::vector<long long> later;
			for (long long t : arrivals)
			{
				if (t <= now && inside < capacity) enter();
				else if (t > now) later.push_back(t);
			}
			arrivals.swap(later);
		}

		void SchoolModel::idle(long long now, int minute, int open, int close, bool holiday,
		                       bool reachable, const std::function<int()> & rnd)
		{
			const bool openHours = minute >= open && minute < close;
			switch (status)
			{
			case Built:
				status = Hiring;
				pattern = 0;
				break;
			case Hiring:
				// CollectEmployee: the 60 places fill from the pool.
				if (reachable) status = Closed;
				break;
			case Closed:
				if (!holiday && reachable && minute >= open && minute < open + 8)
				{
					arrivals.clear();
					for (int k = 0; k < capacity; ++k) arrivals.push_back(now + (rnd() % 6) * 10);
				}
				break;
			case Some:
				if (!openHours)
				{
					// PushOutAll at the 18:00 -> 08:00 window.
					inside = 0;
					arrivals.clear();
					status = Closed;
					pattern = 0;
				}
				else if (minute >= 900) pushOut(3);
				break;
			case Half:
				if ((minute >= 510 && minute < 518) || minute / 60 == 13) { status = InClass; pattern = 2; }
				else if (minute >= 900) pushOut(3);
				break;
			case InClass:
				if (minute / 60 == 12 || minute >= 900) { status = Half; pattern = 3; }
				break;
			}
		}

		// ------------------------------------------------------ Sky Bridge

		int skyBridgeAreaCheck(BuildRect & r, const BuildRect & area, int groundLine,
		                       const std::function<bool(int, int)> & occupied)
		{
			if (groundLine - 4 < r.bottom) return 62;
			const int row = r.bottom - 1;
			int u = r.left - 1;
			while (u >= area.left && !occupied(row, u)) --u;
			if (u < area.left) return 63;
			r.left = u + 1;
			u = r.right;
			while (u <= area.right - 1 && !occupied(row, u)) ++u;
			if (u > area.right - 1) return 63;
			r.right = u;
			for (int y = r.bottom; y < r.bottom + 3 && y < area.bottom; ++y)
				for (int x = r.left; x < r.right; ++x)
					if (occupied(y, x)) return 59;
			for (int y = r.top - 1; y > r.top - 4 && y >= area.top; --y)
				for (int x = r.left; x < r.right; ++x)
					if (occupied(y, x)) return 59;
			if (r.right - r.left < 0x49) return 0;
			return 60;
		}

		std::vector<std::pair<int, int> > skyBridgeSplit(int left, int right, const std::function<bool(int)> & taken)
		{
			std::vector<std::pair<int, int> > runs;
			int x = left;
			while (x < right)
			{
				if (taken(x)) { ++x; continue; }
				const int from = x;
				while (x < right && !taken(x)) ++x;
				runs.push_back(std::make_pair(from, x));
			}
			return runs;
		}

		// ------------------------------------------------- Tonbi / Dolphin

		static void finishTonbi(TonbiDef & d)
		{
			d.minPict = d.maxPict = 0;
			for (const TonbiAnim & a : d.anims)
			{
				if (a.pict == 0) continue;
				if (d.minPict == 0 || a.pict < d.minPict) d.minPict = a.pict;
				d.maxPict = std::max(d.maxPict, a.pict);
			}
		}

		static bool tonbiFromInts(const std::vector<int> & b, TonbiDef & out)
		{
			size_t k = 0;
			auto next = [&](int & v) { if (k >= b.size()) return false; v = b[k++]; return true; };
			TonbiDef d;
			int n = 0;
			if (!next(d.layer) || !next(n) || n < 0 || n > 64) return false;
			if (d.layer > 4) d.layer = 4;
			for (int i = 0; i < n; ++i)
			{
				int row = 0, unit = 0;
				if (!next(row) || !next(unit)) return false;
				d.points.push_back(std::make_pair(row, unit));
			}
			int m = 0;
			if (!next(m) || m < 0 || m > 1024) return false;
			for (int i = 0; i < m; ++i)
			{
				TonbiAnim a;
				if (!next(a.pict) || !next(a.count) || !next(a.dy) || !next(a.dx)) return false;
				d.anims.push_back(a);
			}
			if (d.points.empty() || d.anims.empty()) return false;
			finishTonbi(d);
			out = d;
			return true;
		}

		bool decodeYootTonbi(const YootEventSpec & e, TonbiDef & out)
		{
			return tonbiFromInts(e.body, out);
		}

		TonbiDef builtinYootTonbi(bool dolphin)
		{
			// Kegon.t2p TOEV 128 / Hawai.t2p DOEV 128 bodies.
			static const int kite[] = {1, 3, 5, 123, 6, 111, 7, 102, 14,
				8600, 32, -4, 5, 8601, 1, -4, 4, 8602, 11, 0, 8, 8603, 4, -2, 6, 8604, 5, -8, 0,
				8605, 5, 0, -8, 8606, 20, 1, -7, 8607, 6, 3, -5, 8608, 6, 2, -6, 8609, 6, 0, 8,
				8610, 6, 0, 8, 8611, 5, 1, 7, 0, 1, 3, 12, 8611, 25, 3, 5};
			static const int dolphins[] = {2, 1, 56, 0, 40,
				8901, 1, 0, 8, 8902, 1, 0, 8, 8901, 1, 0, 8, 0, 6, 2, 3, 0, 12, 2, 3, 8903, 1, 0, 0,
				8904, 1, 1, 2, 8905, 1, 0, 2, 8906, 1, 2, -1, 8907, 1, 2, -1, 8908, 1, 2, -2,
				8909, 1, 0, -1, 8910, 1, 1, -6, 8909, 1, 1, -6, 0, 12, 13, 14, 0, 3, 13, 14,
				8911, 1, 0, -1, 8912, 1, -6, 0, 8913, 1, -6, 0, 8914, 1, -6, 0, 8915, 1, -6, 0,
				8901, 1, 0, 0, 0, 1, 5, 12, 8910, 1, -1, -4, 8909, 1, -1, -4, 0, 4, 24, 25,
				8916, 1, -2, -3, 8917, 1, -8, -4, 8917, 3, -18, -4, 8916, 1, 2, -6, 8916, 1, 1, -2,
				8918, 1, 4, -4, 8918, 4, 10, -4, 8909, 1, 3, -6, 8910, 1, 3, -6, 0, 6, 34, 35,
				0, 1, 17, 36, 8909, 1, 0, -6, 8910, 1, 0, -6, 0, 17, 38, 39};
			std::vector<int> b = dolphin ? std::vector<int>(std::begin(dolphins), std::end(dolphins))
			                             : std::vector<int>(std::begin(kite), std::end(kite));
			TonbiDef d;
			tonbiFromInts(b, d);
			return d;
		}

		void TonbiFlight::start(const TonbiDef & def)
		{
			def_ = &def;
			state_.assign(def.anims.size(), std::make_pair(0, 0));
			counter_ = 0;
			mask_ = 0x1ff;
			idx_ = 0;
			visible_ = false;
			x_ = y_ = 0;
			pattern_ = def.minPict;
			steps_ = 0;
		}

		bool TonbiFlight::fly(int i)
		{
			const TonbiAnim & a = def_->anims[i];
			std::pair<int, int> & s = state_[i];
			if (a.pict != 0)
			{
				if (++s.first <= a.count)
				{
					x_ += a.dx;
					y_ += a.dy;
					pattern_ = a.pict;
					++steps_;
				}
			}
			else
			{
				if (s.second == 0) s.second = a.dy;
				const int sub = s.second - 1;
				if (sub >= 0 && sub < (int)def_->anims.size() && sub != i && fly(sub)) ++s.second;
				else if (sub < 0 || sub >= (int)def_->anims.size() || sub == i) ++s.second;
				if (s.second > a.dx) { s.second = 0; ++s.first; }
			}
			if (a.count <= s.first) { s.first = 0; return true; }
			return false;
		}

		bool TonbiFlight::tick(int minuteOfDay, int endMin, bool ready, const std::function<int()> & pick)
		{
			if (!def_) return true;
			if ((++counter_ & mask_) != 1) return false;
			const int n = (int)def_->anims.size();
			if (idx_ == 0)
			{
				if (ready && !def_->points.empty())
				{
					const std::pair<int, int> & p = def_->points[pick() % def_->points.size()];
					x_ = p.second * 8;  // UnitToQD
					y_ = p.first * 36 + 8;
					visible_ = true;
					mask_ = 3;
					idx_ = 1;
				}
				else idx_ = n + 1;
			}
			if (idx_ > n)
			{
				visible_ = false;
				x_ = y_ = 0;
				idx_ = 0;
				mask_ = 0x1ff;
				return minuteOfDay > endMin;
			}
			if (fly(idx_ - 1)) ++idx_;
			return false;
		}

		// ------------------------------------------------------- Mr. Maru

		int maruPhase(int t)
		{
			if (t < kMaruStandMinutes) return 0;
			if (t < kMaruStandMinutes + kMaruDanceMinutes) return 1;
			if (t < 2 * kMaruStandMinutes + kMaruDanceMinutes) return 2;
			return 3;
		}

		// ------------------------------------------------------ hot spring

		bool hotSpringTriggers(bool done, int levelBit, int grade, int population, int threshold, int topFloor)
		{
			if (done || grade < 1 || grade > 31 || !(levelBit & (1 << (grade - 1)))) return false;
			if (population < threshold) return false;
			// area.top >= groundLine: the build's top row is underground,
			// i.e. its highest floor is B1 or lower.
			return topFloor < 0;
		}

		// --------------------------------------------------------- traffic

		void TrafficMap::configure(int left, int top, int right, int bottom)
		{
			left_ = left;
			top_ = top;
			cols_ = std::max(0, (right - left) / 4);
			rows_ = std::max(0, bottom - top);
			cells_.assign((size_t)cols_ * rows_, Cell());
		}

		int TrafficMap::index(int row, int unit) const
		{
			if (row < top_ || row >= top_ + rows_ || unit < left_ || (unit - left_) / 4 >= cols_) return -1;
			const int i = (row - top_) * cols_ + (unit - left_) / 4;
			return i > 0 ? i : -1; // GetUnit 0x1006b800 refuses index <= 0
		}

		const TrafficMap::Cell * TrafficMap::cell(int row, int unit) const
		{
			const int i = index(row, unit);
			return i < 0 ? nullptr : &cells_[i];
		}

		void TrafficMap::pass(int row, int a, int b)
		{
			if (!configured()) return;
			if (a > b) std::swap(a, b);
			for (int u = a - (a - left_) % 4; u <= b; u += 4)
			{
				const int i = index(row, u);
				if (i >= 0) cells_[i].count++;
			}
		}

		int TrafficMap::calcRange(int accum)
		{
			if (accum < 25) return 0;
			if (accum < 125) return 1;
			if (accum < 250) return 2;
			return 3;
		}

		void TrafficMap::hourChanged()
		{
			for (Cell & c : cells_)
			{
				c.accum = (int)(c.accum * 0.96875f) + c.count;
				if (c.accum > 0xffff) c.accum = 0xffff;
				c.count = 0;
				c.range = calcRange(c.accum);
			}
		}

		int TrafficMap::range(int row, int unit) const
		{
			const Cell * c = cell(row, unit);
			return c ? c->range : 0;
		}

		std::string TrafficMap::encode() const
		{
			std::ostringstream o;
			for (size_t i = 0; i < cells_.size(); ++i)
			{
				if (i) o << ',';
				if (cells_[i].count || cells_[i].accum) o << cells_[i].count << ':' << cells_[i].accum;
			}
			return o.str();
		}

		void TrafficMap::decode(const std::string & s)
		{
			size_t i = 0, pos = 0;
			while (pos <= s.size() && i < cells_.size())
			{
				size_t end = s.find(',', pos);
				if (end == std::string::npos) end = s.size();
				const std::string tok = s.substr(pos, end - pos);
				Cell c;
				const size_t colon = tok.find(':');
				if (colon != std::string::npos)
				{
					c.count = std::max(0, atoi(tok.substr(0, colon).c_str()));
					c.accum = std::max(0, std::min(0xffff, atoi(tok.substr(colon + 1).c_str())));
					c.range = calcRange(c.accum);
				}
				cells_[i++] = c;
				pos = end + 1;
			}
		}

		// ------------------------------------------------------------ age

		int yearsSinceBuilt(int totalDays, int buildDay)
		{
			if (buildDay < 0 || totalDays < buildDay) return 0;
			return (int)((unsigned)(totalDays - buildDay) / 12u);
		}

		int floorCharge(int pricePerCell, int emptyCells)
		{
			return pricePerCell * std::max(0, emptyCells);
		}
	}
}
