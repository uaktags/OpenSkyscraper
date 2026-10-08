/* Copyright (c) 2026 OpenSkyscraper contributors */
/// Shared typed-accessor implementations for AbstractPlugin, inherited by the
/// PE Plugin and the folder-based DecompiledPlugin.
#include "AbstractPlugin.h"
#include "WindowsPEExecutable.h"

namespace OT
{
	namespace
	{
		/// Fetches (type, id) and runs `decode` over it.
		template <class T>
		bool decodeVia(const AbstractPlugin & p, const char * type, int id, T & out,
		               bool (*decode)(const char *, size_t, T &))
		{
			const WindowsPEExecutable::Resource * r = p.getResource(type, id);
			if (!r || !r->data) return false;
			return decode(r->data, size_t(r->length), out);
		}
	}

	bool AbstractPlugin::getAttr(PluginAttr & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource("ATTR", 128);
		if (!r) return false;
		return parsePluginAttr(r->data, r->length, out);
	}

	bool AbstractPlugin::getDesc(PluginDesc & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource("DESC", 128);
		if (!r) return false;
		return parsePluginDesc(r->data, r->length, out);
	}

	bool AbstractPlugin::getTemplateDef(TemplateDef & out) const
	{
		// A plugin carries exactly one of the three (plugin-schema §2.3).
		static const struct { const char * type; TemplateDef::Kind kind; } kTypes[] = {
			{"TNDF", TemplateDef::Tenant},
			{"MVDF", TemplateDef::Mover},
			{"OODF", TemplateDef::OuterObj},
		};
		for (const auto & t : kTypes)
		{
			const WindowsPEExecutable::Resource * r = getResource(t.type, 1);
			if (r && r->data)
				return decodeTemplateDef(t.kind, r->data, size_t(r->length), out);
		}
		return false;
	}

	bool AbstractPlugin::getMemberTable(MemberTableDef & out, int id) const
	{
		return decodeVia(*this, "TMDF", id, out, &decodeMemberTableDef);
	}

	std::map<int, MemberTableDef> AbstractPlugin::getMemberTables() const
	{
		std::map<int, MemberTableDef> out;
		const auto byType = resourcesByType();
		auto it = byType.find("TMDF");
		if (it == byType.end()) return out;
		for (int id : it->second)
		{
			MemberTableDef t;
			if (getMemberTable(t, id)) out.emplace(id, std::move(t));
		}
		return out;
	}

	bool AbstractPlugin::getDayParamDef(DayParamDef & out, int id) const
	{
		return decodeVia(*this, "DPDF", id, out, &decodeDayParamDef);
	}

	bool AbstractPlugin::getPlaceParamDef(PlaceParamDef & out, int id) const
	{
		return decodeVia(*this, "PLDF", id, out, &decodePlaceParamDef);
	}

	bool AbstractPlugin::getPeopleAnimeDef(PeopleAnimeDef & out, int id) const
	{
		return decodeVia(*this, "PADF", id, out, &decodePeopleAnimeDef);
	}

	bool AbstractPlugin::getSilhouettes(SilhouetteList & out, int id) const
	{
		return decodeVia(*this, "SLDF", id, out, &decodeSilhouetteList);
	}

	bool AbstractPlugin::getPeopleDefs(PeopleList & out, int id) const
	{
		if (container() == ResourceContainer::SP) return false;  // different layout there
		return decodeVia(*this, "PPDF", id, out, &decodePeopleList);
	}

	bool AbstractPlugin::getWords(WordList & out, int id) const
	{
		return decodeVia(*this, "WODF", id, out, &decodeWordList);
	}

	bool AbstractPlugin::getStringTable(StringTableDef & out, int id) const
	{
		return decodeVia(*this, "STRTABLE", id, out, &decodeStringTableDef)
		    || decodeVia(*this, "STRTBL", id, out, &decodeStringTableDef);
	}

	bool AbstractPlugin::getStringBlock(StringBlock & out, int id) const
	{
		return decodeVia(*this, "6", id, out, &decodeStringBlock);
	}

	bool AbstractPlugin::getVersionInfo(VersionInfo & out) const
	{
		const auto byType = resourcesByType();
		auto it = byType.find("16");
		if (it == byType.end() || it->second.empty()) return false;
		return decodeVia(*this, "16", it->second.front(), out, &decodeVersionInfo);
	}

	bool AbstractPlugin::getMovieDef(MovieDef & out) const
	{
		return decodeVia(*this, "MODF", 128, out, &decodeMovieDef);
	}

	bool AbstractPlugin::getHallEventDef(HallEventDef & out) const
	{
		return decodeVia(*this, "HEDF", 128, out, &decodeHallEventDef);
	}

	bool AbstractPlugin::getAdvertiseDef(AdvertiseDef & out) const
	{
		return decodeVia(*this, "ADDF", 128, out, &decodeAdvertiseDef);
	}

	bool AbstractPlugin::getSubPluginPeopleParamDef(SubPluginPeopleParamDef & out, int id) const
	{
		if (container() != ResourceContainer::SP) return false;
		return decodeVia(*this, "PPDF", id, out, &decodeSubPluginPeopleParamDef);
	}

	bool AbstractPlugin::decodeResource(const std::string & type, int id, DecodedResource & out) const
	{
		const WindowsPEExecutable::Resource * r = getResource(type, id);
		if (!r) return false;
		out = OT::decodeResource(type, id, r->data, size_t(r->length), container());
		return true;
	}
}
