#pragma once
#include <string>

namespace OT
{
	class NameManager
	{
	public:
		static std::string makeName(int personType);
		static void reset();
	};
}
