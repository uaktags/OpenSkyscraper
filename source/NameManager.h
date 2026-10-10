#pragma once
#include <string>

namespace OT
{
	/// Person labels, the 20-slot custom-name registry and the inspector
	/// whereabouts text (batch 5 T44/T45/T47). Pure: no Game dependency.
	class NameManager
	{
	public:
		/// Legacy numbered role names; no longer used for new persons.
		static std::string makeName(int personType);
		static void reset();

		/// Which tenant family a person belongs to (selects the label table).
		enum class Role { HotelGuest, Office, Condo, Visitor, Staff };
		/// Person::Type values the label table needs.
		enum { kTypeHousekeeper = 5, kTypeSecurity = 8 };

		/// Generic label (T44). `hour` is the clock hour for the staff rule.
		static std::string genericLabel(Role role, int member, int personType, double hour);
		/// Priority: custom name, then "VIP", then generic.
		static std::string personLabel(const std::string & custom, bool vip, Role role,
		                               int member, int personType, double hour);

		/// Status line for the inspector (T45): `goingTo` is the person's
		/// destination tag, `floorText` the destination/home floor.
		static std::string whereabouts(const std::string & goingTo, const std::string & homeType,
		                               const std::string & floorText);

		// ---- custom-name registry (T47): 20 slots keyed by (tenant, member) ----
		enum NameClass { kNameOther = 0, kNameHotel = 1, kNameVisitor = 2 };
		static const int kMaxNames = 20;
		static const int kMaxNameLength = 15;
		/// Set, replace (keeps its slot) or, for empty text, delete. Returns
		/// false only when the table is full for a new name.
		static bool setCustomName(const void * tenant, int member, const std::string & text,
		                          int nameClass = kNameOther);
		static std::string customName(const void * tenant, int member);
		static int customNameCount();
		/// Delete every name of a class (hotel names at 17:00, visitors at ft 2500).
		static void purgeClass(int nameClass);
		/// Delete the names of a demolished tenant.
		static void removeTenant(const void * tenant);
	};
}
