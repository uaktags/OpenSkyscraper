#pragma once
#include <tinyxml2.h>

namespace OT {
	// tinyxml2's two-argument Attribute(name, value) is a *match* test, not a
	// default: it returns NULL unless the attribute equals `value`. Use this
	// helper to read a string attribute with a fallback (old saves lack new
	// attributes and must still load).
	inline const char *xmlAttrOr(const tinyxml2::XMLElement &el, const char *name, const char *fallback)
	{
		const char *v = el.Attribute(name);
		return v ? v : fallback;
	}
}
