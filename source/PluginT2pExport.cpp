/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginT2pExport.h"

#include "OspPlugin.h"
#include "PluginResourceCodec.h"
#include "SubPluginDefs.h"

#include <algorithm>
#include <fstream>
#include <map>

namespace OT
{
	namespace
	{
		uint16_t rd16(const std::string & s, size_t p)
		{
			return uint16_t(uint8_t(s[p]) | (uint8_t(s[p + 1]) << 8));
		}
		uint32_t rd32(const std::string & s, size_t p)
		{
			return uint32_t(uint8_t(s[p])) | (uint32_t(uint8_t(s[p + 1])) << 8)
			     | (uint32_t(uint8_t(s[p + 2])) << 16) | (uint32_t(uint8_t(s[p + 3])) << 24);
		}
		void wr16(std::string & s, size_t p, uint16_t v)
		{
			s[p] = char(v & 0xFF);
			s[p + 1] = char(v >> 8);
		}
		void wr32(std::string & s, size_t p, uint32_t v)
		{
			for (int i = 0; i < 4; ++i) s[p + size_t(i)] = char((v >> (8 * i)) & 0xFF);
		}
		uint32_t alignUp(uint32_t v, uint32_t a) { return a ? (v + a - 1) / a * a : v; }

		bool fail(std::string * error, const std::string & msg)
		{
			if (error) *error = msg;
			return false;
		}

		struct DirHdr
		{
			uint32_t characteristics = 0, timestamp = 0;
			uint16_t major = 0, minor = 0;
		};

		struct Leaf
		{
			uint32_t lang = 0, codepage = 0, reserved = 0;
			std::string data;
			bool original = false;
			uint32_t originalRva = 0;
			std::string padAfter;     // original alignment bytes after the payload (not always zero)
			uint32_t originalSize = 0, originalEntryOff = 0;
			bool changed = false;
			// layout
			uint32_t entryOff = 0, dataOff = 0;
		};

		struct NameNode
		{
			bool named = false;
			std::u16string name;
			uint32_t id = 0;
			DirHdr hdr;
			std::vector<Leaf> langs;
			uint32_t dirOff = 0;
		};

		struct TypeNode
		{
			bool named = false;
			std::u16string name;
			uint32_t id = 0;
			DirHdr hdr;
			std::vector<NameNode> names;
			uint32_t dirOff = 0;

			std::string key() const
			{
				if (!named) return std::to_string(id);
				std::string s;
				for (char16_t c : name) s.push_back(char(c));
				return s;
			}
		};

		struct Section
		{
			size_t headerOff = 0;
			uint32_t virtualSize = 0, virtualAddress = 0, rawSize = 0, rawPtr = 0;
		};

		/// Reads one directory table; entries are (nameOrId, target).
		bool readDir(const std::string & rs, uint32_t off, DirHdr & h, std::vector<std::pair<uint32_t, uint32_t>> & ents)
		{
			if (size_t(off) + 16 > rs.size()) return false;
			h.characteristics = rd32(rs, off);
			h.timestamp = rd32(rs, off + 4);
			h.major = rd16(rs, off + 8);
			h.minor = rd16(rs, off + 10);
			const size_t n = size_t(rd16(rs, off + 12)) + rd16(rs, off + 14);
			if (size_t(off) + 16 + 8 * n > rs.size()) return false;
			ents.clear();
			for (size_t k = 0; k < n; ++k)
				ents.emplace_back(rd32(rs, off + 16 + 8 * k), rd32(rs, off + 20 + 8 * k));
			return true;
		}

		bool readName(const std::string & rs, uint32_t off, std::u16string & out)
		{
			if (size_t(off) + 2 > rs.size()) return false;
			const size_t n = rd16(rs, off);
			if (size_t(off) + 2 + 2 * n > rs.size()) return false;
			out.clear();
			for (size_t k = 0; k < n; ++k) out.push_back(char16_t(rd16(rs, off + 2 + 2 * k)));
			return true;
		}

		void writeDir(std::string & out, uint32_t off, const DirHdr & h, uint16_t named, uint16_t ids)
		{
			wr32(out, off, h.characteristics);
			wr32(out, off + 4, h.timestamp);
			wr16(out, off + 8, h.major);
			wr16(out, off + 10, h.minor);
			wr16(out, off + 12, named);
			wr16(out, off + 14, ids);
		}

		bool nameLess(const std::u16string & a, const std::u16string & b) { return a < b; }
	}

	uint32_t computePeChecksum(const std::string & pe)
	{
		if (pe.size() < 0x40) return 0;
		const uint32_t lfanew = rd32(pe, 0x3C);
		const size_t ckOff = size_t(lfanew) + 4 + 20 + 64;
		uint64_t sum = 0;
		for (size_t p = 0; p < pe.size(); p += 2)
		{
			if (p == ckOff || p == ckOff + 2) continue;
			uint32_t w = uint8_t(pe[p]);
			if (p + 1 < pe.size()) w |= uint32_t(uint8_t(pe[p + 1])) << 8;
			sum += w;
			sum = (sum & 0xFFFF) + (sum >> 16);
		}
		sum = (sum & 0xFFFF) + (sum >> 16);
		return uint32_t(sum) + uint32_t(pe.size());
	}

	bool rebuildPeResources(const std::string & pe, const std::vector<PeResourceData> & resources,
	                        std::string & out, std::string * error)
	{
		// ---- headers ----
		if (pe.size() < 0x40 || pe[0] != 'M' || pe[1] != 'Z') return fail(error, "not an MZ image");
		const uint32_t lfanew = rd32(pe, 0x3C);
		if (size_t(lfanew) + 24 > pe.size() || rd32(pe, lfanew) != 0x00004550) return fail(error, "no PE signature");
		const size_t coff = size_t(lfanew) + 4;
		const uint16_t numSections = rd16(pe, coff + 2);
		const uint16_t optSize = rd16(pe, coff + 16);
		const size_t opt = coff + 20;
		if (opt + optSize > pe.size() || rd16(pe, opt) != 0x10b) return fail(error, "not a PE32 image");
		const uint32_t sectAlign = rd32(pe, opt + 32);
		const uint32_t fileAlign = rd32(pe, opt + 36);
		const uint32_t numDirs = rd32(pe, opt + 92);
		const size_t dirs = opt + 96;
		if (numDirs <= 2) return fail(error, "image has no resource directory");
		const uint32_t rsrcRva = rd32(pe, dirs + 2 * 8);
		const size_t secTable = opt + optSize;
		if (secTable + 40 * size_t(numSections) > pe.size()) return fail(error, "truncated section table");
		std::vector<Section> sections(numSections);
		int rsrcIdx = -1;
		for (size_t k = 0; k < numSections; ++k)
		{
			Section & s = sections[k];
			s.headerOff = secTable + 40 * k;
			s.virtualSize = rd32(pe, s.headerOff + 8);
			s.virtualAddress = rd32(pe, s.headerOff + 12);
			s.rawSize = rd32(pe, s.headerOff + 16);
			s.rawPtr = rd32(pe, s.headerOff + 20);
			if (s.virtualAddress == rsrcRva) rsrcIdx = int(k);
		}
		if (rsrcIdx < 0) return fail(error, "the resource directory does not start a section");
		const Section rsec = sections[size_t(rsrcIdx)];
		if (size_t(rsec.rawPtr) + rsec.rawSize > pe.size()) return fail(error, "truncated .rsrc");
		for (size_t k = size_t(rsrcIdx) + 1; k < sections.size(); ++k)
			if (sections[k].rawPtr < rsec.rawPtr + rsec.rawSize || sections[k].virtualAddress < rsec.virtualAddress)
				return fail(error, "unexpected section order after .rsrc");
		const std::string rs = pe.substr(rsec.rawPtr, rsec.rawSize);

		// ---- parse the original tree ----
		DirHdr rootHdr;
		std::vector<TypeNode> types;
		std::vector<std::pair<uint32_t, std::u16string>> strings; // (offset, text)
		{
			std::vector<std::pair<uint32_t, uint32_t>> te;
			if (!readDir(rs, 0, rootHdr, te)) return fail(error, "bad root resource directory");
			for (const auto & t : te)
			{
				TypeNode tn;
				if (t.first & 0x80000000u)
				{
					tn.named = true;
					if (!readName(rs, t.first & 0x7FFFFFFFu, tn.name)) return fail(error, "bad type name");
					strings.emplace_back(t.first & 0x7FFFFFFFu, tn.name);
				}
				else tn.id = t.first;
				if (!(t.second & 0x80000000u)) return fail(error, "type entry without a directory");
				std::vector<std::pair<uint32_t, uint32_t>> ne;
				if (!readDir(rs, t.second & 0x7FFFFFFFu, tn.hdr, ne)) return fail(error, "bad name directory");
				for (const auto & n : ne)
				{
					NameNode nn;
					if (n.first & 0x80000000u) return fail(error, "named resource ids are not supported");
					nn.id = n.first;
					if (!(n.second & 0x80000000u)) return fail(error, "name entry without a directory");
					std::vector<std::pair<uint32_t, uint32_t>> le;
					if (!readDir(rs, n.second & 0x7FFFFFFFu, nn.hdr, le)) return fail(error, "bad language directory");
					for (const auto & l : le)
					{
						if (l.second & 0x80000000u) return fail(error, "unexpected fourth directory level");
						const uint32_t de = l.second;
						if (size_t(de) + 16 > rs.size()) return fail(error, "bad data entry");
						Leaf leaf;
						leaf.lang = l.first;
						leaf.originalRva = rd32(rs, de);
						const uint32_t size = rd32(rs, de + 4);
						leaf.codepage = rd32(rs, de + 8);
						leaf.reserved = rd32(rs, de + 12);
						const uint32_t doff = leaf.originalRva - rsec.virtualAddress;
						if (leaf.originalRva < rsec.virtualAddress || size_t(doff) + size > rs.size())
							return fail(error, "resource data outside .rsrc");
						leaf.data = rs.substr(doff, size);
						leaf.original = true;
						leaf.originalSize = size;
						leaf.originalEntryOff = de;
						const uint32_t end = doff + size;
						if (alignUp(end, 8) <= rs.size()) leaf.padAfter = rs.substr(end, alignUp(end, 8) - end);
						nn.langs.push_back(std::move(leaf));
					}
					if (nn.langs.size() != 1) return fail(error, "resources with several languages are not supported");
					tn.names.push_back(std::move(nn));
				}
				types.push_back(std::move(tn));
			}
		}
		std::sort(strings.begin(), strings.end());

		// ---- apply the new resource set ----
		std::map<std::pair<std::string, int>, const PeResourceData *> want;
		for (const PeResourceData & r : resources)
		{
			if (r.type.empty()) return fail(error, "resource with an empty type");
			if (!want.emplace(std::make_pair(r.type, r.id), &r).second)
				return fail(error, "duplicate resource " + r.type + " " + std::to_string(r.id));
		}
		std::map<std::pair<std::string, int>, bool> placed;
		size_t removed = 0;
		for (TypeNode & t : types)
		{
			std::vector<NameNode> kept;
			for (NameNode & n : t.names)
			{
				auto it = want.find({t.key(), int(n.id)});
				if (it == want.end()) { ++removed; continue; }
				if (n.langs[0].data != it->second->data)
				{
					n.langs[0].data = it->second->data;
					n.langs[0].changed = true;
				}
				placed[it->first] = true;
				kept.push_back(std::move(n));
			}
			t.names = std::move(kept);
		}
		const bool sameSet = removed == 0 && placed.size() == resources.size();
		types.erase(std::remove_if(types.begin(), types.end(), [](const TypeNode & t) { return t.names.empty(); }),
		            types.end());
		for (const PeResourceData & r : resources)
		{
			if (placed.count({r.type, r.id})) continue;
			const bool numeric = std::all_of(r.type.begin(), r.type.end(), [](char c) { return c >= '0' && c <= '9'; });
			auto tit = std::find_if(types.begin(), types.end(), [&](const TypeNode & t) { return t.key() == r.type; });
			if (tit == types.end())
			{
				TypeNode tn;
				tn.named = !numeric;
				if (numeric) tn.id = uint32_t(std::stoul(r.type));
				else for (char c : r.type) tn.name.push_back(char16_t(uint8_t(c)));
				// Named entries first (sorted), then ids ascending.
				auto pos = std::find_if(types.begin(), types.end(), [&](const TypeNode & o) {
					if (tn.named) return !o.named || nameLess(tn.name, o.name);
					return !o.named && tn.id < o.id;
				});
				tit = types.insert(pos, std::move(tn));
			}
			NameNode nn;
			nn.id = uint32_t(r.id);
			Leaf leaf;
			leaf.data = r.data;
			nn.langs.push_back(std::move(leaf));
			auto pos = std::find_if(tit->names.begin(), tit->names.end(), [&](const NameNode & o) { return nn.id < o.id; });
			tit->names.insert(pos, std::move(nn));
		}

		uint32_t newVirtualSize = 0, newRawSize = 0;
		uint32_t dirSize = rd32(pe, dirs + 2 * 8 + 4);
		std::string sec;
		if (sameSet)
		{
			// Same resource set: keep the original section byte for byte
			// (linkers leave gaps and stale bytes we cannot regenerate) and
			// patch the changed payloads in place, or append them when they
			// outgrow their slot.
			sec = rs;
			uint32_t appendEnd = rsec.virtualSize;
			for (TypeNode & t : types)
				for (NameNode & n : t.names)
					for (Leaf & l : n.langs)
					{
						if (!l.changed) continue;
						const uint32_t doff = l.originalRva - rsec.virtualAddress;
						const uint32_t slot = alignUp(l.originalSize, 8);
						if (l.data.size() <= slot)
						{
							sec.replace(doff, l.data.size(), l.data);
							std::fill(sec.begin() + long(doff + l.data.size()), sec.begin() + long(doff + slot), '\0');
							wr32(sec, l.originalEntryOff + 4, uint32_t(l.data.size()));
						}
						else
						{
							const uint32_t at = alignUp(appendEnd, 8);
							if (sec.size() < size_t(at) + l.data.size()) sec.resize(size_t(at) + l.data.size(), '\0');
							sec.replace(at, l.data.size(), l.data);
							wr32(sec, l.originalEntryOff, rsec.virtualAddress + at);
							wr32(sec, l.originalEntryOff + 4, uint32_t(l.data.size()));
							appendEnd = at + uint32_t(l.data.size());
						}
					}
			newVirtualSize = appendEnd > rsec.virtualSize ? alignUp(appendEnd, 8) : rsec.virtualSize;
			newRawSize = std::max(rsec.rawSize, alignUp(newVirtualSize, fileAlign));
			sec.resize(newRawSize, '\0');
			dirSize += newVirtualSize - rsec.virtualSize;
		}
		else
		{
		// ---- layout ----
		uint32_t off = 16 + 8 * uint32_t(types.size());
		for (TypeNode & t : types)
		{
			t.dirOff = off;
			off += 16 + 8 * uint32_t(t.names.size());
		}
		for (TypeNode & t : types)
			for (NameNode & n : t.names)
			{
				n.dirOff = off;
				off += 16 + 8 * uint32_t(n.langs.size());
			}
		for (TypeNode & t : types)
			for (NameNode & n : t.names)
				for (Leaf & l : n.langs)
				{
					l.entryOff = off;
					off += 16;
				}
		// Strings: original order first, then new names.
		std::vector<std::u16string> strOrder;
		for (const auto & s : strings)
			if (std::find(strOrder.begin(), strOrder.end(), s.second) == strOrder.end()) strOrder.push_back(s.second);
		for (const TypeNode & t : types)
			if (t.named && std::find(strOrder.begin(), strOrder.end(), t.name) == strOrder.end()) strOrder.push_back(t.name);
		std::map<std::u16string, uint32_t> strOff;
		for (const std::u16string & s : strOrder)
		{
			bool used = false;
			for (const TypeNode & t : types) used = used || (t.named && t.name == s);
			if (!used) continue;
			strOff[s] = off;
			off += 2 + 2 * uint32_t(s.size());
		}
		// Payloads: originals in their original order, then the new ones.
		std::vector<Leaf *> order;
		for (TypeNode & t : types)
			for (NameNode & n : t.names)
				for (Leaf & l : n.langs) order.push_back(&l);
		std::stable_sort(order.begin(), order.end(), [](const Leaf * a, const Leaf * b) {
			if (a->original != b->original) return a->original;
			return a->original && a->originalRva < b->originalRva;
		});
		for (Leaf * l : order)
		{
			off = alignUp(off, 8);
			l->dataOff = off;
			off += uint32_t(l->data.size());
		}
		newVirtualSize = alignUp(off, 8);
		newRawSize = alignUp(newVirtualSize, fileAlign);
		dirSize = newVirtualSize;

		// ---- write the section ----
		sec.assign(newRawSize, '\0');
		{
			uint16_t named = 0, ids = 0;
			for (const TypeNode & t : types) (t.named ? named : ids)++;
			writeDir(sec, 0, rootHdr, named, ids);
			for (size_t k = 0; k < types.size(); ++k)
			{
				const TypeNode & t = types[k];
				wr32(sec, 16 + 8 * k, t.named ? (0x80000000u | strOff[t.name]) : t.id);
				wr32(sec, 20 + 8 * k, 0x80000000u | t.dirOff);
				writeDir(sec, t.dirOff, t.hdr, 0, uint16_t(t.names.size()));
				for (size_t i = 0; i < t.names.size(); ++i)
				{
					const NameNode & n = t.names[i];
					wr32(sec, t.dirOff + 16 + 8 * i, n.id);
					wr32(sec, t.dirOff + 20 + 8 * i, 0x80000000u | n.dirOff);
					writeDir(sec, n.dirOff, n.hdr, 0, uint16_t(n.langs.size()));
					for (size_t li = 0; li < n.langs.size(); ++li)
					{
						const Leaf & l = n.langs[li];
						wr32(sec, n.dirOff + 16 + 8 * li, l.lang);
						wr32(sec, n.dirOff + 20 + 8 * li, l.entryOff);
						wr32(sec, l.entryOff, rsec.virtualAddress + l.dataOff);
						wr32(sec, l.entryOff + 4, uint32_t(l.data.size()));
						wr32(sec, l.entryOff + 8, l.codepage);
						wr32(sec, l.entryOff + 12, l.reserved);
						sec.replace(l.dataOff, l.data.size(), l.data);
						const uint32_t end = l.dataOff + uint32_t(l.data.size());
						if (l.padAfter.size() == alignUp(end, 8) - end && end + l.padAfter.size() <= sec.size())
							sec.replace(end, l.padAfter.size(), l.padAfter);
					}
				}
			}
			for (const auto & s : strOff)
			{
				wr16(sec, s.second, uint16_t(s.first.size()));
				for (size_t k = 0; k < s.first.size(); ++k) wr16(sec, s.second + 2 + 2 * k, uint16_t(s.first[k]));
			}
		}

		}

		// ---- assemble the image ----
		const int64_t rawDelta = int64_t(newRawSize) - int64_t(rsec.rawSize);
		const int64_t vaDelta = int64_t(alignUp(newVirtualSize, sectAlign)) - int64_t(alignUp(rsec.virtualSize, sectAlign));
		const uint32_t oldRawEnd = rsec.rawPtr + rsec.rawSize;
		const uint32_t oldVaEnd = rsec.virtualAddress + alignUp(rsec.virtualSize, sectAlign);
		std::string img = pe.substr(0, rsec.rawPtr) + sec + pe.substr(oldRawEnd);

		wr32(img, rsec.headerOff + 8, newVirtualSize);
		wr32(img, rsec.headerOff + 16, newRawSize);
		wr32(img, dirs + 2 * 8 + 4, dirSize);
		for (size_t k = size_t(rsrcIdx) + 1; k < sections.size(); ++k)
		{
			wr32(img, sections[k].headerOff + 12, uint32_t(int64_t(sections[k].virtualAddress) + vaDelta));
			wr32(img, sections[k].headerOff + 20, uint32_t(int64_t(sections[k].rawPtr) + rawDelta));
		}
		for (uint32_t d = 0; d < numDirs && d < 16; ++d)
		{
			if (d == 2 || d == 4) continue;  // 4 = security: a file offset, handled below
			const uint32_t rva = rd32(img, dirs + 8 * d);
			if (rva >= oldVaEnd) wr32(img, dirs + 8 * d, uint32_t(int64_t(rva) + vaDelta));
		}
		if (numDirs > 4)
		{
			const uint32_t secOff = rd32(img, dirs + 8 * 4);
			if (secOff >= oldRawEnd) wr32(img, dirs + 8 * 4, uint32_t(int64_t(secOff) + rawDelta));
		}
		wr32(img, opt + 56, uint32_t(int64_t(rd32(img, opt + 56)) + vaDelta));      // SizeOfImage
		wr32(img, opt + 8, uint32_t(int64_t(rd32(img, opt + 8)) + rawDelta));       // SizeOfInitializedData
		{
			const uint32_t sym = rd32(img, coff + 8);
			if (sym >= oldRawEnd) wr32(img, coff + 8, uint32_t(int64_t(sym) + rawDelta));
		}
		// Debug directory entries carry file offsets (the CodeView data sits
		// in the overlay after the last section).
		if (numDirs > 6)
		{
			const uint32_t dbgRva = rd32(img, dirs + 8 * 6);
			const uint32_t dbgSize = rd32(img, dirs + 8 * 6 + 4);
			// Find the (new) file offset of the debug directory.
			size_t dbgFile = 0;
			for (size_t k = 0; k < sections.size(); ++k)
			{
				const uint32_t va = rd32(img, sections[k].headerOff + 12);
				const uint32_t vs = std::max(rd32(img, sections[k].headerOff + 8), rd32(img, sections[k].headerOff + 16));
				if (dbgRva >= va && dbgRva < va + vs)
				{
					dbgFile = rd32(img, sections[k].headerOff + 20) + (dbgRva - va);
					break;
				}
			}
			if (dbgFile && dbgSize && dbgFile + dbgSize <= img.size())
			{
				for (size_t e = 0; e + 28 <= dbgSize; e += 28)
				{
					const size_t p = dbgFile + e;
					const uint32_t addr = rd32(img, p + 20);
					const uint32_t ptr = rd32(img, p + 24);
					if (addr >= oldVaEnd) wr32(img, p + 20, uint32_t(int64_t(addr) + vaDelta));
					if (ptr >= oldRawEnd) wr32(img, p + 24, uint32_t(int64_t(ptr) + rawDelta));
				}
			}
		}
		if (rd32(pe, opt + 64) != 0) wr32(img, opt + 64, computePeChecksum(img));
		out = std::move(img);
		return true;
	}

	bool exportOspToT2p(const OspPlugin & p, std::string & out, std::string * error)
	{
		if (p.resources.empty())
			return fail(error, "this plugin has no YootTower resources (it was not converted from a .t2p)");
		if (p.container == ResourceContainer::SP)
		{
			SpContainer c;
			for (const OspResource & r : p.resources)
			{
				SpContainer::Entry e;
				e.type = r.spType;
				e.id = uint32_t(r.res.id);
				if (!encodeResource(r.res, e.data))
					return fail(error, "cannot encode " + r.res.type + " " + std::to_string(r.res.id));
				c.entries.push_back(std::move(e));
			}
			if (!encodeSpContainer(c, out)) return fail(error, "cannot encode the SP container");
			return true;
		}
		if (p.peImage.empty())
			return fail(error, "the original DLL image is missing (pe/original.t2p)");
		std::vector<PeResourceData> res;
		for (const OspResource & r : p.resources)
		{
			PeResourceData d;
			d.type = r.res.type;
			d.id = r.res.id;
			if (!encodeResource(r.res, d.data))
				return fail(error, "cannot encode " + resourceTypeDisplayName(r.res.type) + " " + std::to_string(r.res.id));
			res.push_back(std::move(d));
		}
		return rebuildPeResources(p.peImage, res, out, error);
	}

	bool writeT2pFile(const OspPlugin & p, const std::string & path, std::string * error)
	{
		std::string bytes;
		if (!exportOspToT2p(p, bytes, error)) return false;
		std::ofstream f(path, std::ios::binary | std::ios::trunc);
		if (!f) return fail(error, "cannot write " + path);
		f.write(bytes.data(), std::streamsize(bytes.size()));
		if (!f.good()) return fail(error, "cannot write " + path);
		return true;
	}
}
