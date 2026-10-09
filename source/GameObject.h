#pragma once

#include <functional>

namespace OT {
	class Application;
	class Game;

	namespace Sim { bool deterministic(); }

	class GameObject
	{
	public:
		Application * const app;
		Game * const game;
		/// Creation order, used to order pointer-keyed containers during a
		/// deterministic headless run (see SimLess). A copy is a new object
		/// and gets its own serial.
		const unsigned long long simSerial;
		GameObject(Game * game);
		GameObject(const GameObject & other);
	};

	/// Comparator for sets/maps keyed by GameObject pointers. Normal play
	/// keeps plain pointer order; a deterministic --sim run orders by
	/// creation serial, so iteration (and the rand() draws it drives) does
	/// not depend on where the heap placed each object.
	struct SimLess
	{
		template <class T>
		bool operator()(const T * a, const T * b) const
		{
			if (!Sim::deterministic())
				return std::less<const T *>()(a, b);
			return a->simSerial < b->simSerial;
		}
	};
}
