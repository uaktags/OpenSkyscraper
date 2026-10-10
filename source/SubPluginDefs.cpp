/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "SubPluginDefs.h"

#include <utility>

namespace OT
{
	namespace
	{
		uint32_t readLE32(const unsigned char * p)
		{
			return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
		}

		void appendLE32(std::string & s, uint32_t v)
		{
			for (int i = 0; i < 4; ++i) s.push_back(char((v >> (8 * i)) & 0xFF));
		}

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

		void readHeader(CResReader & r, SubPluginHeader & h)
		{
			h.title = r.str();
			h.tieupDate = r.u32();
		}

		void writeHeader(CResWriter & w, const SubPluginHeader & h)
		{
			w.str(h.title);
			w.u32(h.tieupDate);
		}
	}

	// -------------------------------------------------------------------------
	// SP container (OpenSubPluginFS 0x10012570)
	// -------------------------------------------------------------------------

	std::string SpContainer::Entry::typeName() const
	{
		if (type == 0) return "BITMAP";
		// The tag constant's big-endian bytes are the engine name ('MODF').
		std::string s;
		for (int i = 3; i >= 0; --i)
		{
			const char c = char((type >> (8 * i)) & 0xFF);
			if (c != '\0') s.push_back(c);
		}
		return s;
	}

	const SpContainer::Entry * SpContainer::find(const std::string & typeName, uint32_t id) const
	{
		for (const Entry & e : entries)
			if (e.id == id && e.typeName() == typeName) return &e;
		return nullptr;
	}

	bool decodeSpContainer(const char * data, size_t length, SpContainer & out)
	{
		if (!data || length < 6 || data[0] != 'S' || data[1] != 'P') return false;
		const unsigned char * b = reinterpret_cast<const unsigned char *>(data);
		const uint32_t n = readLE32(b + 2);
		if (n > (length - 6) / 16) return false;
		SpContainer c;
		c.entries.resize(n);
		for (uint32_t k = 0; k < n; ++k)
		{
			const unsigned char * e = b + 6 + 16 * k;
			SpContainer::Entry & en = c.entries[k];
			en.type = readLE32(e);
			en.id = readLE32(e + 4);
			en.offset = readLE32(e + 8);
			const uint32_t size = readLE32(e + 12);
			if (uint64_t(en.offset) + size > length) return false;
			en.data.assign(data + en.offset, size);
		}
		out = std::move(c);
		return true;
	}

	bool encodeSpContainer(const SpContainer & c, std::string & out)
	{
		std::string hdr = "SP";
		appendLE32(hdr, uint32_t(c.entries.size()));
		std::string body;
		const size_t base = 6 + 16 * c.entries.size();
		for (const SpContainer::Entry & e : c.entries)
		{
			appendLE32(hdr, e.type);
			appendLE32(hdr, e.id);
			appendLE32(hdr, uint32_t(base + body.size()));
			appendLE32(hdr, uint32_t(e.data.size()));
			body += e.data;
		}
		out = hdr + body;
		return true;
	}

	// -------------------------------------------------------------------------
	// MODF
	// -------------------------------------------------------------------------

	bool decodeMovieDef(const char * data, size_t length, MovieDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, MovieDef & d) {
			readHeader(r, d.header);
			d.imageID = r.i32();
			d.filmKind = r.i32();
			d.phase1Months = r.i32();
			d.phase2Months = r.i32();
			d.phase3Months = r.i32();
			d.phase4Months = r.i32();
			d.cycleMonths = r.i32();
			d.production = r.str();
			d.director = r.str();
			d.starring = r.str();
			d.runningTime = r.i32();
			d.unkFC = r.i32();
			d.description = r.str();
			d.customerTableID = r.i32();
			d.unk104 = r.i32();
			d.unk108 = r.i32();
			d.famousAudience = r.i32();
			d.remakeAudience = r.i32();
			d.unk114 = r.i32();
			d.extra.resize(size_t(r.count()));
			for (std::string & s : d.extra)
			{
				s = r.str();
				if (!r.ok()) return;
			}
			d.stillImageID = r.i32();
			d.clipSeconds = r.i32();
			d.movieFile = r.str();
		});
	}

	bool encodeMovieDef(const MovieDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const MovieDef & d) {
			writeHeader(w, d.header);
			w.i32(d.imageID);
			w.i32(d.filmKind);
			w.i32(d.phase1Months);
			w.i32(d.phase2Months);
			w.i32(d.phase3Months);
			w.i32(d.phase4Months);
			w.i32(d.cycleMonths);
			w.str(d.production);
			w.str(d.director);
			w.str(d.starring);
			w.i32(d.runningTime);
			w.i32(d.unkFC);
			w.str(d.description);
			w.i32(d.customerTableID);
			w.i32(d.unk104);
			w.i32(d.unk108);
			w.i32(d.famousAudience);
			w.i32(d.remakeAudience);
			w.i32(d.unk114);
			w.count(d.extra.size());
			for (const std::string & s : d.extra) w.str(s);
			w.i32(d.stillImageID);
			w.i32(d.clipSeconds);
			w.str(d.movieFile);
		});
	}

	// -------------------------------------------------------------------------
	// HEDF
	// -------------------------------------------------------------------------

	bool decodeHallEventDef(const char * data, size_t length, HallEventDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, HallEventDef & d) {
			readHeader(r, d.header);
			d.imageID = r.i32();
			d.unkCC = r.u32();
			d.bookingDays = r.u32();
			d.openTime = r.u32();
			d.closeTime = r.u32();
			d.monthMask = r.u32();
			d.cost = r.i32();
			d.customerTableID = r.i32();
			d.unkDC = r.i32();
			d.unkE0 = r.i32();
			d.unkE4 = r.i32();
			d.bindTime = r.i32();
			d.stillImageID = r.i32();
			d.description = r.str();
			d.movieFile = r.str();
		});
	}

	bool encodeHallEventDef(const HallEventDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const HallEventDef & d) {
			writeHeader(w, d.header);
			w.i32(d.imageID);
			w.u32(d.unkCC);
			w.u32(d.bookingDays);
			w.u32(d.openTime);
			w.u32(d.closeTime);
			w.u32(d.monthMask);
			w.i32(d.cost);
			w.i32(d.customerTableID);
			w.i32(d.unkDC);
			w.i32(d.unkE0);
			w.i32(d.unkE4);
			w.i32(d.bindTime);
			w.i32(d.stillImageID);
			w.str(d.description);
			w.str(d.movieFile);
		});
	}

	// -------------------------------------------------------------------------
	// ADDF
	// -------------------------------------------------------------------------

	bool decodeAdvertiseDef(const char * data, size_t length, AdvertiseDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, AdvertiseDef & d) {
			readHeader(r, d.header);
			d.subType = r.fourcc();
			d.imageID = r.i32();
			d.unkD0 = r.u32();
			d.days = r.u32();
			d.money = r.i32();
			d.path = r.str();
		});
	}

	bool encodeAdvertiseDef(const AdvertiseDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const AdvertiseDef & d) {
			writeHeader(w, d.header);
			w.fourcc(d.subType);
			w.i32(d.imageID);
			w.u32(d.unkD0);
			w.u32(d.days);
			w.i32(d.money);
			w.str(d.path);
		});
	}

	// -------------------------------------------------------------------------
	// SP PPDF
	// -------------------------------------------------------------------------

	bool decodeSubPluginPeopleParamDef(const char * data, size_t length, SubPluginPeopleParamDef & out)
	{
		return decodeStrict(data, length, out, [](CResReader & r, SubPluginPeopleParamDef & d) {
			for (int32_t & v : d.values) v = r.i32();
		});
	}

	bool encodeSubPluginPeopleParamDef(const SubPluginPeopleParamDef & def, std::string & out)
	{
		return encodeWith(def, out, [](CResWriter & w, const SubPluginPeopleParamDef & d) {
			for (int32_t v : d.values) w.i32(v);
		});
	}
}
