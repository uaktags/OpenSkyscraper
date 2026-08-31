/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Parser + writer for the YootTower OBJMAP sprite-layout resource.
///
/// OBJMAP is a tab-separated, CRLF-delimited ASCII text format that declares
/// how a plugin's RT_BITMAP sprite sheet is sliced into named parts (toolbox
/// icons, primary views, walking-people frames, ...). The parser is a thin
/// line-based state machine; the writer emits the canonical grammar so that
/// parse(write(parse(x))) is identity.
///
/// Pure functions over byte buffers (no Plugin / PE dependency), mirroring the
/// style of PluginResources. Field layout: doc/yoottower/objmap-format.md.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	/// A pixel rectangle in (left, top, right, bottom) order, matching the
	/// on-disk OBJMAP `Rect x1,y1,x2,y2` encoding. Width/height derive from
	/// the deltas; the stored form round-trips byte-faithfully.
	struct ObjmapRect
	{
		int left = 0, top = 0, right = 0, bottom = 0;

		int width() const  { return right - left; }
		int height() const { return bottom - top; }
		bool operator==(const ObjmapRect & o) const
		{ return left == o.left && top == o.top && right == o.right && bottom == o.bottom; }
	};

	/// One `DefParts` block. `id`/`pattern`/`span` default to -1 ("absent");
	/// a real OBJMAP only ever carries a subset of these per block.
	struct ObjmapBlock
	{
		std::string name;                  // semantic name: "Category", "View", "<Item>", "Csr", ...
		int id      = -1;                  // ID attribute (101/100/103/...)
		int pattern = -1;                  // Pattern attribute (frame index / state)
		int span    = -1;                  // Span attribute (multi-story variant)
		ObjmapRect rect;                   // from the List attribute; zero if absent
		std::vector<std::string> flags;    // trailing tokens: "Transparent", "Halftone", ...
	};

	/// Parses an OBJMAP resource into a list of blocks. Returns false (leaving
	/// `out` untouched) if `data` doesn't begin with the `DefParts` magic or is
	/// empty. Trailing junk, missing flags, and attribute order are tolerated.
	bool parsePluginObjmap(const char * data, int length, std::vector<ObjmapBlock> & out);

	/// Emits the canonical OBJMAP text for `blocks` (DefParts headers,
	/// `\t\tkey\tvalue` attribute lines, `List\tRect ...` lines, blank-line
	/// separators, terminating `End`). CRLF line endings, tab field separator
	/// — matches the on-disk grammar so a parse/write/parse round-trip is an
	/// identity for the subset of attributes we model.
	std::string writePluginObjmap(const std::vector<ObjmapBlock> & blocks);

	/// Finds the first block whose `name` and `pattern` both match (pattern
	/// compared only when `pattern >= 0`). Returns nullptr if none. Used by
	/// callers looking up "the selected/unselected toolbox icon", etc.
	ObjmapBlock * findObjmapBlock(std::vector<ObjmapBlock> & blocks,
	                              const std::string & name, int pattern = -1);
	inline const ObjmapBlock * findObjmapBlock(const std::vector<ObjmapBlock> & blocks,
	                                           const std::string & name, int pattern = -1)
	{
		// const overload via delegation to the mutable one.
		return findObjmapBlock(const_cast<std::vector<ObjmapBlock> &>(blocks), name, pattern);
	}
}
