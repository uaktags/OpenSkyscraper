/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once

#include <tinyxml2.h>

namespace OT
{
	/// String attribute with a fallback. tinyxml2's two-argument
	/// Attribute(name, value) is a *match* test (it returns NULL unless the
	/// attribute equals `value`), not a default, so assigning its result to
	/// a std::string crashes whenever the attribute holds anything else.
	/// (yoot branch name; kept so YootTower loader code compiles unchanged)
	inline const char * xmlAttributeOr(const tinyxml2::XMLElement & e, const char * name, const char * fallback)
	{
		const char * value = e.Attribute(name);
		return value ? value : fallback;
	}

	// tinyxml2's two-argument Attribute(name, value) is a *match* test, not a
	// default: it returns NULL unless the attribute equals `value`. Use this
	// helper to read a string attribute with a fallback (old saves lack new
	// attributes and must still load).
	// (art branch name; kept so SimTower save/loader code compiles unchanged)
	inline const char * xmlAttrOr(const tinyxml2::XMLElement & el, const char * name, const char * fallback)
	{
		return xmlAttributeOr(el, name, fallback);
	}
}
