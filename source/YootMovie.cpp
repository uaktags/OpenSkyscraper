/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootMovie.h"

#include <fstream>
#include <iterator>

namespace OT
{
	namespace
	{
		uint32_t be32(const unsigned char * p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
		uint16_t be16(const unsigned char * p) { return uint16_t((p[0] << 8) | p[1]); }

		struct Atom
		{
			const unsigned char * body = nullptr;
			size_t size = 0;      ///< body size
			std::string type;
		};

		/// Children of a container body; stops at the first malformed atom.
		std::vector<Atom> children(const unsigned char * p, size_t n)
		{
			std::vector<Atom> out;
			size_t off = 0;
			while (off + 8 <= n)
			{
				uint64_t size = be32(p + off);
				size_t header = 8;
				if (size == 1)
				{
					if (off + 16 > n) break;
					size = (uint64_t(be32(p + off + 8)) << 32) | be32(p + off + 12);
					header = 16;
				}
				else if (size == 0)
					size = n - off;
				if (size < header || size > n - off) break;
				Atom a;
				a.type.assign(reinterpret_cast<const char *>(p + off + 4), 4);
				a.body = p + off + header;
				a.size = size_t(size) - header;
				out.push_back(a);
				off += size_t(size);
			}
			return out;
		}

		const Atom * find(const std::vector<Atom> & atoms, const char * type)
		{
			for (const Atom & a : atoms)
				if (a.type == type) return &a;
			return nullptr;
		}

		struct SoundTable
		{
			std::string format;
			unsigned channels = 0, bits = 0, rate = 0;
			std::vector<std::pair<uint32_t, uint32_t> > stsc; ///< {firstChunk (1-based), samplesPerChunk}
			std::vector<uint64_t> chunks;
		};

		bool readStbl(const Atom & stbl, SoundTable & t)
		{
			const std::vector<Atom> kids = children(stbl.body, stbl.size);
			const Atom * stsd = find(kids, "stsd");
			const Atom * stsc = find(kids, "stsc");
			const Atom * stco = find(kids, "stco");
			const Atom * co64 = find(kids, "co64");
			if (!stsd || !stsc || (!stco && !co64)) return false;
			// stsd: v/f, count, entry {size, format, 6 reserved, dref(2),
			// version(2), revision(2), vendor(4), channels(2), bits(2),
			// compressionId(2), packetSize(2), rate(16.16)}.
			if (stsd->size < 8 + 36) return false;
			const unsigned char * e = stsd->body + 8;
			t.format.assign(reinterpret_cast<const char *>(e + 4), 4);
			t.channels = be16(e + 24);
			t.bits = be16(e + 26);
			t.rate = be32(e + 32) >> 16;
			if (stsc->size < 8) return false;
			const uint32_t n = be32(stsc->body + 4);
			if (stsc->size < 8 + size_t(n) * 12) return false;
			for (uint32_t i = 0; i < n; ++i)
				t.stsc.push_back(std::make_pair(be32(stsc->body + 8 + i * 12), be32(stsc->body + 12 + i * 12)));
			const Atom * co = stco ? stco : co64;
			const size_t w = stco ? 4 : 8;
			if (co->size < 8) return false;
			const uint32_t m = be32(co->body + 4);
			if (co->size < 8 + size_t(m) * w) return false;
			for (uint32_t i = 0; i < m; ++i)
			{
				const unsigned char * q = co->body + 8 + i * w;
				t.chunks.push_back(stco ? be32(q) : ((uint64_t(be32(q)) << 32) | be32(q + 4)));
			}
			return true;
		}
	}

	bool decodeQuickTimeSound(const char * data, size_t length, YootMovieSound & out)
	{
		const unsigned char * p = reinterpret_cast<const unsigned char *>(data);
		const std::vector<Atom> top = children(p, length);
		const Atom * moov = find(top, "moov");
		if (!moov) return false;
		for (const Atom & trak : children(moov->body, moov->size))
		{
			if (trak.type != "trak") continue;
			const std::vector<Atom> t0 = children(trak.body, trak.size);
			const Atom * mdia = find(t0, "mdia");
			if (!mdia) continue;
			const std::vector<Atom> m = children(mdia->body, mdia->size);
			const Atom * hdlr = find(m, "hdlr");
			const Atom * minf = find(m, "minf");
			if (!hdlr || !minf || hdlr->size < 12 || std::string(reinterpret_cast<const char *>(hdlr->body + 8), 4) != "soun")
				continue;
			const std::vector<Atom> m0 = children(minf->body, minf->size);
			const Atom * stbl = find(m0, "stbl");
			SoundTable t;
			if (!stbl || !readStbl(*stbl, t)) continue;
			const bool raw = t.format == "raw ";
			if ((!raw && t.format != "twos") || (t.bits != 8 && t.bits != 16) || t.channels < 1 || t.channels > 2 || t.rate == 0)
				continue;

			YootMovieSound s;
			s.channels = t.channels;
			s.sampleRate = t.rate;
			const size_t frameBytes = size_t(t.channels) * (t.bits / 8);
			for (size_t c = 0; c < t.chunks.size(); ++c)
			{
				// Samples (PCM frames) in chunk c+1: the last stsc run that
				// starts at or before it.
				uint32_t perChunk = 0;
				for (const auto & run : t.stsc)
					if (run.first <= c + 1) perChunk = run.second;
				const uint64_t off = t.chunks[c];
				const uint64_t bytes = uint64_t(perChunk) * frameBytes;
				if (off > length || bytes > length - off) return false;
				const unsigned char * q = p + off;
				if (t.bits == 8)
					for (uint64_t k = 0; k < bytes; ++k)
						s.samples.push_back(raw ? int16_t((int(q[k]) - 128) << 8) : int16_t(int8_t(q[k]) * 256));
				else
					for (uint64_t k = 0; k + 1 < bytes; k += 2)
						s.samples.push_back(int16_t(be16(q + k)));
			}
			if (s.samples.empty()) continue;
			out = s;
			return true;
		}
		return false;
	}

	bool loadQuickTimeSound(const std::string & path, YootMovieSound & out)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f) return false;
		const std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		return decodeQuickTimeSound(bytes.data(), bytes.size(), out);
	}
}
