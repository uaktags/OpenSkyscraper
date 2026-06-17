#include "NameManager.h"
#include <vector>

namespace OT
{
	static std::vector<int> counters(9, 0);

	static const char* roleName(int t)
	{
		switch (t)
		{
			case 0: return "Man";
			case 1: return "Salesman";
			case 2: return "Woman";
			case 3: return "Child";
			case 4: return "Woman";
			case 5: return "Housekeeper";
			case 6: return "Woman";
			case 7: return "Woman";
			case 8: return "Security";
			default: return "Person";
		}
	}

	std::string NameManager::makeName(int personType)
	{
		if (personType < 0 || personType >= (int)counters.size())
			return "Person";
		int n = ++counters[personType];
		char buf[64];
		snprintf(buf, sizeof(buf), "%s #%d", roleName(personType), n);
		return std::string(buf);
	}

	void NameManager::reset()
	{
		for (size_t i = 0; i < counters.size(); ++i)
			counters[i] = 0;
	}
}
