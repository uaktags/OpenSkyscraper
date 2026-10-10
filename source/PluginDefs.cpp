/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginDefs.h"

#include <utility>

namespace OT
{
	namespace
	{
		/// Runs `read(reader, tmp)` over a binary CResFile resource and commits
		/// to `out` only if every byte was consumed without error.
		template <class T, class Fn>
		bool decodeStrict(const char * data, size_t length, T & out, Fn read)
		{
			CResReader r(data, length);
			T tmp{};
			read(r, tmp);
			if (!r.ok() || !r.atEnd()) return false;
			out = std::move(tmp);
			return true;
		}

		template <class T, class Fn>
		bool encodeWith(const T & in, std::string & out, Fn write)
		{
			CResWriter w;
			write(w, in);
			return w.finish(out);
		}

		// Element counts above this are treated as corruption.
		constexpr int32_t kMaxVariations = 64;
		constexpr int32_t kMaxElems = 100000;

		// ---- ToolDef (T2ToolDef 0x10065540) ----
		void readToolDef(CResReader & r, ToolDefPart & t)
		{
			t.toolType = r.i32();
			t.width = r.i32();
			t.height = r.i32();
			t.level = r.i32();
			t.attribute = r.u32();
			t.categoryHelp = r.str();
			t.categoryName = r.str();
			t.category = r.i32();
			t.toolNo = r.i32();
			t.skip1 = r.i32();
			t.skip2 = r.i32();
			t.subPluginType = r.fourcc();
			t.skip3 = r.i32();
			t.skip4 = r.i32();
			const int32_t n = r.count(kMaxVariations);
			t.variations.resize(size_t(n));
			for (ToolVariation & v : t.variations)
			{
				v.priceString = r.str();
				v.outMoneyString = r.str();
				v.help = r.str();
				v.helpTitle = r.str();
				v.name = r.str();
				v.price = r.i32();
				v.outMoney = r.i32();
				v.skip5 = r.i32();
				v.skip6 = r.i32();
				v.skip7 = r.i32();
				v.consumptionPower = r.i32();
				if (!r.ok()) return;
			}
		}

		void writeToolDef(CResWriter & w, const ToolDefPart & t)
		{
			w.i32(t.toolType);
			w.i32(t.width);
			w.i32(t.height);
			w.i32(t.level);
			w.u32(t.attribute);
			w.str(t.categoryHelp);
			w.str(t.categoryName);
			w.i32(t.category);
			w.i32(t.toolNo);
			w.i32(t.skip1);
			w.i32(t.skip2);
			w.fourcc(t.subPluginType);
			w.i32(t.skip3);
			w.i32(t.skip4);
			w.count(t.variations.size());
			for (const ToolVariation & v : t.variations)
			{
				w.str(v.priceString);
				w.str(v.outMoneyString);
				w.str(v.help);
				w.str(v.helpTitle);
				w.str(v.name);
				w.i32(v.price);
				w.i32(v.outMoney);
				w.i32(v.skip5);
				w.i32(v.skip6);
				w.i32(v.skip7);
				w.i32(v.consumptionPower);
			}
		}

		// ---- EquipDef (T2EquipDef 0x1002f240) ----
		// The per-variation records are counted by the ToolDef mValiationCnt.
		void readEquipDef(CResReader & r, size_t variationCount, EquipDefPart & e)
		{
			e.capacity = r.i32();
			e.stress = r.i32();
			e.skip8 = r.i32();
			e.variations.resize(variationCount);
			for (EquipVariation & v : e.variations)
			{
				v.outSpan = r.i32();
				v.skip9 = r.i32();
				v.skip10 = r.i32();
			}
			e.inSpan = r.i32();
			e.buildDays = r.i32();
			const int32_t n = r.count(kMaxElems);
			e.inMoney.resize(size_t(n));
			for (int32_t & m : e.inMoney)
			{
				m = r.i32();
				if (!r.ok()) return;
			}
		}

		void writeEquipDef(CResWriter & w, size_t variationCount, const EquipDefPart & e)
		{
			if (e.variations.size() != variationCount) { w.fail(); return; }
			w.i32(e.capacity);
			w.i32(e.stress);
			w.i32(e.skip8);
			for (const EquipVariation & v : e.variations)
			{
				w.i32(v.outSpan);
				w.i32(v.skip9);
				w.i32(v.skip10);
			}
			w.i32(e.inSpan);
			w.i32(e.buildDays);
			w.count(e.inMoney.size());
			for (int32_t m : e.inMoney) w.i32(m);
		}

		// ---- TenantDef (T2TenantDef 0x100623c0) ----
		void readTenantDef(CResReader & r, TenantDefPart & t)
		{
			t.registID = r.u32();
			t.downFlag = r.u32();
			t.numOfKind = r.i32();
			t.numOfPattern = r.i32();
			t.belongCapacity = r.i32();
			t.senseOfNoise = r.i32();
			t.entranceV = r.i32();
			t.entranceH = r.i32();
			t.exitV = r.i32();
			t.exitH = r.i32();
			t.openTime = r.i32();
			t.closeTime = r.i32();
			t.bindTime = r.i32();
			t.maxBuildCount = r.i32();
			t.skip11 = r.u32();
			t.skip12 = r.u32();
			t.skip13 = r.u32();
			t.movieName = r.str();
		}

		void writeTenantDef(CResWriter & w, const TenantDefPart & t)
		{
			w.u32(t.registID);
			w.u32(t.downFlag);
			w.i32(t.numOfKind);
			w.i32(t.numOfPattern);
			w.i32(t.belongCapacity);
			w.i32(t.senseOfNoise);
			w.i32(t.entranceV);
			w.i32(t.entranceH);
			w.i32(t.exitV);
			w.i32(t.exitH);
			w.i32(t.openTime);
			w.i32(t.closeTime);
			w.i32(t.bindTime);
			w.i32(t.maxBuildCount);
			w.u32(t.skip11);
			w.u32(t.skip12);
			w.u32(t.skip13);
			w.str(t.movieName);
		}

		// ---- MoverDef (T2MoverDef 0x1003ffb0) ----
		void readMoverDef(CResReader & r, MoverDefPart & m)
		{
			m.addSize = r.rect();
			m.maxLength = r.i32();
			m.minLength = r.i32();
			m.maxStop = r.i32();
			m.score = r.i32();
			m.defaultWait = r.i32();
			for (int32_t & s : m.speed) s = r.i32();
			m.maxModule = r.i32();
			m.moduleUnitH = r.i32();
			m.moduleUnitV = r.i32();
			m.moduleParam1 = r.i32();
			m.moduleParam2 = r.i32();
			m.moduleInset = r.rect();
			m.skip14 = r.i32();
			m.skip15 = r.i32();
			m.skip16 = r.i32();
			m.skip17 = r.i32();
		}

		void writeMoverDef(CResWriter & w, const MoverDefPart & m)
		{
			w.rect(m.addSize);
			w.i32(m.maxLength);
			w.i32(m.minLength);
			w.i32(m.maxStop);
			w.i32(m.score);
			w.i32(m.defaultWait);
			for (int32_t s : m.speed) w.i32(s);
			w.i32(m.maxModule);
			w.i32(m.moduleUnitH);
			w.i32(m.moduleUnitV);
			w.i32(m.moduleParam1);
			w.i32(m.moduleParam2);
			w.rect(m.moduleInset);
			w.i32(m.skip14);
			w.i32(m.skip15);
			w.i32(m.skip16);
			w.i32(m.skip17);
		}
	}

	// -------------------------------------------------------------------------
	// TemplateDef
	// -------------------------------------------------------------------------

	const char * TemplateDef::resourceType() const
	{
		switch (kind)
		{
			case Mover:    return "MVDF";
			case OuterObj: return "OODF";
			default:       return "TNDF";
		}
	}

	bool decodeTemplateDef(TemplateDef::Kind kind, const char * data, size_t length, TemplateDef & out)
	{
		return decodeStrict(data, length, out, [kind](CResReader & r, TemplateDef & d) {
			d.kind = kind;
			readToolDef(r, d.tool);
			if (!r.ok()) return;
			if (kind == TemplateDef::OuterObj)
			{
				d.unreadA = r.i32();
				d.unreadB = r.i32();
				return;
			}
			readEquipDef(r, d.tool.variations.size(), d.equip);
			if (kind == TemplateDef::Tenant) readTenantDef(r, d.tenant);
			else                             readMoverDef(r, d.mover);
		});
	}

	bool encodeTemplateDef(const TemplateDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const TemplateDef & d) {
			writeToolDef(w, d.tool);
			if (d.kind == TemplateDef::OuterObj)
			{
				w.i32(d.unreadA);
				w.i32(d.unreadB);
				return;
			}
			writeEquipDef(w, d.tool.variations.size(), d.equip);
			if (d.kind == TemplateDef::Tenant) writeTenantDef(w, d.tenant);
			else                               writeMoverDef(w, d.mover);
		});
	}

	// -------------------------------------------------------------------------
	// TMDF (T2TenantMemberTableDef 0x100630a0)
	// -------------------------------------------------------------------------

	bool decodeMemberTableDef(const char * data, size_t length, MemberTableDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, MemberTableDef & d) {
			d.econoType = r.u32();
			d.specialFlag = r.u32();
			d.dayParamID = r.i32();
			d.placeParamID = r.i32();
			const int32_t n = r.count(kMaxElems);
			d.members.resize(size_t(n));
			for (MemberDef & m : d.members)
			{
				m.name = r.str();
				m.demandType = r.i32();
				m.startSilhouetteType = r.i32();
				m.endSilhouetteType = r.i32();
				m.optionType = r.i32();
				m.percent = r.u32();
				m.numOfPeople = r.u32();
				if (!r.ok()) return;
			}
		});
	}

	bool encodeMemberTableDef(const MemberTableDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const MemberTableDef & d) {
			w.u32(d.econoType);
			w.u32(d.specialFlag);
			w.i32(d.dayParamID);
			w.i32(d.placeParamID);
			w.count(d.members.size());
			for (const MemberDef & m : d.members)
			{
				w.str(m.name);
				w.i32(m.demandType);
				w.i32(m.startSilhouetteType);
				w.i32(m.endSilhouetteType);
				w.i32(m.optionType);
				w.u32(m.percent);
				w.u32(m.numOfPeople);
			}
		});
	}

	// -------------------------------------------------------------------------
	// DPDF / PLDF / PADF
	// -------------------------------------------------------------------------

	bool decodeDayParamDef(const char * data, size_t length, DayParamDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, DayParamDef & d) {
			for (auto & day : d.percent)
				for (uint32_t & v : day) v = r.u32();
		});
	}

	bool encodeDayParamDef(const DayParamDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const DayParamDef & d) {
			for (const auto & day : d.percent)
				for (uint32_t v : day) w.u32(v);
		});
	}

	bool decodePlaceParamDef(const char * data, size_t length, PlaceParamDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, PlaceParamDef & d) {
			for (PlaceParamDef::Place & p : d.places)
			{
				p.percent = r.u32();
				p.scoreSlot = r.i32();
			}
		});
	}

	bool encodePlaceParamDef(const PlaceParamDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const PlaceParamDef & d) {
			for (const PlaceParamDef::Place & p : d.places)
			{
				w.u32(p.percent);
				w.i32(p.scoreSlot);
			}
		});
	}

	bool decodePeopleAnimeDef(const char * data, size_t length, PeopleAnimeDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, PeopleAnimeDef & d) {
			d.param1 = r.i32();
			d.param2 = r.i32();
			const int32_t n = r.count(kMaxElems);
			d.elems.resize(size_t(n));
			for (PeopleAnimeDef::Elem & e : d.elems)
			{
				e.position = r.i32();
				e.pattern = r.i32();
				e.silhouette = r.i32();
				e.animeID = r.i32();
				if (!r.ok()) return;
			}
		});
	}

	bool encodePeopleAnimeDef(const PeopleAnimeDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const PeopleAnimeDef & d) {
			w.i32(d.param1);
			w.i32(d.param2);
			w.count(d.elems.size());
			for (const PeopleAnimeDef::Elem & e : d.elems)
			{
				w.i32(e.position);
				w.i32(e.pattern);
				w.i32(e.silhouette);
				w.i32(e.animeID);
			}
		});
	}

	// -------------------------------------------------------------------------
	// SLDF / PPDF / WODF / STRTABLE (int32 count prefix + records)
	// -------------------------------------------------------------------------

	bool decodeSilhouetteList(const char * data, size_t length, SilhouetteList & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, SilhouetteList & l) {
			l.resize(size_t(r.count(kMaxElems)));
			for (SilhouetteDef & s : l)
			{
				s.silhouetteType = r.i32();
				s.width = r.i32();
				s.routeType = r.i32();
				s.bitmapID = r.i32();
				s.name = r.str();
				if (!r.ok()) return;
			}
		});
	}

	bool encodeSilhouetteList(const SilhouetteList & list, std::string & out)
	{
		return encodeWith(list, out, [](CResWriter & w, const SilhouetteList & l) {
			w.count(l.size());
			for (const SilhouetteDef & s : l)
			{
				w.i32(s.silhouetteType);
				w.i32(s.width);
				w.i32(s.routeType);
				w.i32(s.bitmapID);
				w.str(s.name);
			}
		});
	}

	bool decodePeopleList(const char * data, size_t length, PeopleList & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, PeopleList & l) {
			l.resize(size_t(r.count(kMaxElems)));
			for (PeopleDef & p : l)
			{
				p.peopleType = r.i32();
				p.attribute = r.i32();
				p.silhouetteType = r.i32();
				p.stressMax = r.i32();
				p.stressBlueLimit = r.i32();
				p.stressYellowLimit = r.i32();
				if (!r.ok()) return;
			}
		});
	}

	bool encodePeopleList(const PeopleList & list, std::string & out)
	{
		return encodeWith(list, out, [](CResWriter & w, const PeopleList & l) {
			w.count(l.size());
			for (const PeopleDef & p : l)
			{
				w.i32(p.peopleType);
				w.i32(p.attribute);
				w.i32(p.silhouetteType);
				w.i32(p.stressMax);
				w.i32(p.stressBlueLimit);
				w.i32(p.stressYellowLimit);
			}
		});
	}

	bool decodeWordList(const char * data, size_t length, WordList & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, WordList & l) {
			l.resize(size_t(r.count(kMaxElems)));
			for (WordDef & wd : l)
			{
				wd.sexMask = r.u32();
				wd.ageMask = r.u32();
				wd.flags = r.u32();
				wd.special = r.u32();
				wd.text = r.str();
				if (!r.ok()) return;
			}
		});
	}

	bool encodeWordList(const WordList & list, std::string & out)
	{
		return encodeWith(list, out, [](CResWriter & w, const WordList & l) {
			w.count(l.size());
			for (const WordDef & wd : l)
			{
				w.u32(wd.sexMask);
				w.u32(wd.ageMask);
				w.u32(wd.flags);
				w.u32(wd.special);
				w.str(wd.text);
			}
		});
	}

	bool decodeStringTableDef(const char * data, size_t length, StringTableDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, StringTableDef & l) {
			l.resize(size_t(r.count(kMaxElems)));
			for (std::string & s : l)
			{
				s = r.str();
				if (!r.ok()) return;
			}
		});
	}

	bool encodeStringTableDef(const StringTableDef & list, std::string & out)
	{
		return encodeWith(list, out, [](CResWriter & w, const StringTableDef & l) {
			w.count(l.size());
			for (const std::string & s : l) w.str(s);
		});
	}
}
