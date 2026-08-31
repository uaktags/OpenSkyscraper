/* Copyright (c) 2026 OpenSkyscraper Project */
#include "SaveHash.h"
#include <cstdint>
#include <cctype>
#include <cstdlib>

namespace OT {

namespace SaveHash {

	const char * const DEV_SAVE_SALT = "openskyweb-dev-salt";

	namespace {
		std::string computeSaveHashSalted(const std::string & xmlWithoutHash)
		{
			std::string salted(DEV_SAVE_SALT);
			salted += '\0';
			salted += xmlWithoutHash;
			return fnv1aHex(salted);
		}
	}

	std::string fnv1aHex(const std::string & s)
	{
		uint32_t h = 0x811c9dc5u;
		for (size_t i = 0; i < s.size(); i++) {
			h ^= (uint32_t)(unsigned char)s[i];
			h *= 0x01000193u;
		}
		char buf[16];
		snprintf(buf, sizeof(buf), "%08x", h);
		return std::string(buf);
	}

	std::string computeSaveHash(const std::string & xmlWithoutHash)
	{
		return computeSaveHashSalted(xmlWithoutHash);
	}

	std::string injectSaveHash(const std::string & xml)
	{
		static const std::string tag = "<tower";
		size_t idx = xml.find(tag);
		if (idx == std::string::npos)
			return xml;
		const std::string hash = computeSaveHash(xml);
		return xml.substr(0, idx + tag.size()) + " hash=\"" + hash + "\"" + xml.substr(idx + tag.size());
	}

	VerifyResult verifySaveHash(const std::string & text)
	{
		VerifyResult r;
		r.valid = true;
		r.legacy = true;

		static const std::string attrStart = "<tower hash=\"";
		size_t pos = text.find(attrStart);
		if (pos == std::string::npos)
			return r; // legacy save without integrity hash

		size_t valueStart = pos + attrStart.size();
		size_t valueEnd = text.find('"', valueStart);
		if (valueEnd == std::string::npos)
			return r; // malformed attribute; treat as legacy rather than failing

		r.legacy = false;
		r.stored = text.substr(valueStart, valueEnd - valueStart);
		// Lowercase the stored hash so uppercase variants still verify.
		for (size_t i = 0; i < r.stored.size(); i++)
			r.stored[i] = (char)tolower((unsigned char)r.stored[i]);

		std::string withoutHash = text.substr(0, pos) + "<tower" + text.substr(valueEnd + 1);
		r.computed = computeSaveHash(withoutHash);
		r.valid = (r.stored == r.computed);
		return r;
	}

}
}
