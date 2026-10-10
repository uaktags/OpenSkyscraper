/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once

namespace OT {
	namespace Yoot {
		/// The door cycle of a YootTower elevator car, one mover idle at a time
		/// (T2.exe runs every mover four times a game minute). It mirrors the
		/// three states of StdElev / BigElev / ServElev after a car reaches a
		/// stop: state 1/2 unload (BigElev FUN_1000c150), state 3 boarding
		/// (FUN_1000c2c0) and state 4 hold (FUN_1000c540). Unload and boarding
		/// start at counter 6; on an even counter one rider steps off the car
		/// (or out of the queue), on counter 2 every remaining one does, on an
		/// odd counter the counter just ticks. The hold counts down from the
		/// mover's waitCount, which the original runs as 6 whatever the file
		/// says (an export of 3, 9 and 1 all came back as 6 in the first save);
		/// a call appointed to the car during the hold reopens the boarding.
		/// The cost for a car to serve a call at position `p` (BigElev FUN_1000da60,
		/// the same function as StdElev's FUN_1000d070). Positions count from the
		/// shaft's bottom, `top` is the last one, `T` the car's next stop (module
		/// +0x68), `cur` its position. carDir: 0 parked, 1 sweeping up, 2 sweeping
		/// down. A parked car pays the distance; a sweeping car pays by the table
		/// below and ignores how far it has already travelled towards T.
		inline int sweepCost(int carDir, int cur, int T, int p, bool callDown, int top)
		{
			if (carDir == 0) return p < cur ? cur - p : p - cur;
			if (carDir == 1)
			{
				if (callDown) return 2 * top - T - p;
				return p < T ? p + 2 * top - T : p - T;
			}
			if (callDown) return p <= T ? T - p : (2 * top - p) + T;
			return T + p;
		}

		struct DoorCycle
		{
			enum Phase { kNone = 0, kUnload = 1, kBoard = 2, kHold = 3 };
			static const int kWindow = 6;   ///< counter at the start of unload / boarding
			static const int kHoldIdles = 6; ///< waitCount as measured in the original

			Phase phase = kNone;
			int counter = 0;

			/// What the car sees on this idle.
			struct Ctx
			{
				int leaving = 0;      ///< riders whose destination is this floor
				bool request = false; ///< a call appointed to this car exists here
				int waiting = 0;      ///< people queued at that call
				int riders = 0;       ///< riders in the car
				int capacity = 0;     ///< car capacity
			};
			/// What the car does on this idle.
			struct Out
			{
				int off = 0;          ///< 0 none, 1 one rider steps off, 2 every remaining rider
				int on = 0;           ///< 0 none, 1 one rider boards, 2 every waiting rider that fits
				bool endStop = false; ///< the call loses its appointment (ClearOnStop)
				bool depart = false;  ///< the stop is over (FUN_1000cb00)
			};

			/// A car reached its target floor (FUN_1000cbd0). `stopHere`: riders
			/// leave here or a call is appointed to the car. Returns false when
			/// the car goes straight on.
			bool arrive(bool leaves, bool callHere)
			{
				if (!leaves && !callHere) { phase = kNone; counter = 0; return false; }
				phase = leaves ? kUnload : kBoard;
				counter = kWindow;
				return true;
			}

			Out step(const Ctx & c)
			{
				Out out;
				const bool even = (counter & 1) == 0;
				if (phase == kUnload)
				{
					if (even)
					{
						if (c.leaving > 0) { out.off = counter == 2 ? 2 : 1; --counter; }
						else { phase = kBoard; counter = kWindow; }
					}
					else if (--counter == 0) { phase = kBoard; counter = kWindow; }
				}
				else if (phase == kBoard)
				{
					if (!c.request) { phase = kHold; counter = kHoldIdles; }
					else if (even)
					{
						if (c.riders >= c.capacity) { out.endStop = out.depart = true; }
						else if (c.waiting > 0) { out.on = counter == 2 ? 2 : 1; --counter; }
						else if (counter != kWindow) { out.endStop = true; phase = kHold; counter = kHoldIdles; }
						else out.endStop = out.depart = true;
					}
					else
					{
						--counter;
						if (c.riders >= c.capacity) out.endStop = out.depart = true;
						else if (counter == 0) { out.endStop = true; phase = kHold; counter = kHoldIdles; }
					}
				}
				else if (phase == kHold)
				{
					if (c.request && c.waiting > 0) { phase = kBoard; counter = kWindow; }
					else if (--counter < 1) out.endStop = out.depart = true;
				}
				if (out.depart) { phase = kNone; counter = 0; }
				return out;
			}
		};
	}
}
