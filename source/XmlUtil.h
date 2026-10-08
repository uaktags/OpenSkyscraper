/* Copyright (c) 2026 OpenSkyscraper contributors */
#pragma once

#include <tinyxml2.h>

namespace OT
{
	/// String attribute with a fallback. tinyxml2's two-argument
	/// Attribute(name, value) is a *match* test (it returns NULL unless the
	/// attribute equals `value`), not a default, so assigning its result to
	/// a std::string crashes whenever the attribute holds anything else.
	inline const char * xmlAttributeOr(const tinyxml2::XMLElement & e, const char * name, const char * fallback)
	{
		const char * value = e.Attribute(name);
		return value ? value : fallback;
	}
}
