#include "NameManager.h"
#include <cstdio>
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
		for (int i = 0; i < kMaxNames; ++i) { /* registry cleared below */ }
		purgeClass(kNameOther); purgeClass(kNameHotel); purgeClass(kNameVisitor);
	}

	std::string NameManager::genericLabel(Role role, int member, int personType, double hour)
	{
		if (personType == kTypeSecurity) return "Security";
		if (personType == kTypeHousekeeper) return hour < 17.0 ? "Housekeeper" : "Janitor";
		switch (role)
		{
			case Role::HotelGuest:
				return member == 2 ? "Woman" : "Man";
			case Role::Office:
				if (member == 0 || member == 1) return "Salesman";
				if (member == 2 || member == 3) return "Man";
				return "Woman";
			case Role::Condo:
				return member == 1 ? "Mother with Baby" : member == 2 ? "Child" : "Man";
			case Role::Visitor:
			{
				const int m = member & 7;
				if (m == 1 || m == 3) return "Woman";
				if (m == 5) return "Woman with Kid";
				if (m == 7) return "Mother with Baby";
				return "Man";
			}
			case Role::Staff:
				return "Security";
		}
		return "Man";
	}

	std::string NameManager::personLabel(const std::string & custom, bool vip, Role role,
	                                     int member, int personType, double hour)
	{
		if (!custom.empty()) return custom;
		if (vip) return "VIP";
		return genericLabel(role, member, personType, hour);
	}

	std::string NameManager::whereabouts(const std::string & goingTo, const std::string & homeType,
	                                     const std::string & floorText)
	{
		if (goingTo == "Lobby" || goingTo == "Lobby to eat" || goingTo == "Lobby to leave" ||
		    goingTo == "Parking Space" || goingTo == "Lobby to go home")
			return goingTo;
		if (goingTo.compare(0, 14, "Medical Center") == 0)
			return "Medical Center, " + floorText;
		if (!goingTo.empty())
			return goingTo + ", " + floorText;
		return homeType + ", " + floorText;
	}

	namespace {
		struct NameEntry
		{
			bool used = false;
			const void * tenant = nullptr;
			int member = 0;
			int nameClass = 0;
			char text[16] = {0};
		};
		NameEntry registry[NameManager::kMaxNames];
	}

	bool NameManager::setCustomName(const void * tenant, int member, const std::string & text,
	                                int nameClass)
	{
		for (int i = 0; i < kMaxNames; ++i)
		{
			NameEntry & e = registry[i];
			if (!e.used || e.tenant != tenant || e.member != member) continue;
			if (text.empty()) { e = NameEntry(); return true; }
			snprintf(e.text, sizeof(e.text), "%.15s", text.c_str());
			e.nameClass = nameClass;
			return true;
		}
		if (text.empty()) return true;
		for (int i = 0; i < kMaxNames; ++i)
		{
			NameEntry & e = registry[i];
			if (e.used) continue;
			e.used = true; e.tenant = tenant; e.member = member; e.nameClass = nameClass;
			snprintf(e.text, sizeof(e.text), "%.15s", text.c_str());
			return true;
		}
		return false;
	}

	std::string NameManager::customName(const void * tenant, int member)
	{
		for (int i = 0; i < kMaxNames; ++i)
			if (registry[i].used && registry[i].tenant == tenant && registry[i].member == member)
				return registry[i].text;
		return std::string();
	}

	int NameManager::customNameCount()
	{
		int n = 0;
		for (int i = 0; i < kMaxNames; ++i) if (registry[i].used) ++n;
		return n;
	}

	void NameManager::purgeClass(int nameClass)
	{
		for (int i = 0; i < kMaxNames; ++i)
			if (registry[i].used && registry[i].nameClass == nameClass) registry[i] = NameEntry();
	}

	void NameManager::removeTenant(const void * tenant)
	{
		for (int i = 0; i < kMaxNames; ++i)
			if (registry[i].used && registry[i].tenant == tenant) registry[i] = NameEntry();
	}
}
