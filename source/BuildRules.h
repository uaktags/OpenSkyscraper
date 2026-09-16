/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once

#include <string>

namespace OT {

/**
 * Pure placement policy for a named tower ruleset.
 *
 * Horizontal footprints are [x, x + width). A ruleset without a known fixed
 * boundary permits every representable positive footprint; a World plugin may
 * add its scenario boundary in a later integration layer.
 */
class BuildRules
{
public:
	enum class Ruleset
	{
		SimTower,
		OpenSky,
		YootTower
	};

	explicit BuildRules(Ruleset ruleset);

	Ruleset ruleset() const;

	/// Stable, parseable identifier suitable for persistence and configuration.
	const char * identifier() const;
	const char * displayName() const;
	bool hasFixedHorizontalBoundary() const;

	/// Returns false for nonpositive or non-representable [x, x + width) spans.
	bool allowsHorizontalFootprint(int x, int width) const;

	/// Parses only the stable identifiers returned by identifier().
	static bool parseIdentifier(const std::string & identifier, Ruleset & ruleset);

private:
	Ruleset m_ruleset;
};

} // namespace OT
