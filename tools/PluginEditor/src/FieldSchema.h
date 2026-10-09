/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Editor metadata for the Json fields of every resource type: captions,
/// tooltips, validated ranges, enum choices, bit names and which fields are
/// "advanced" (read but unused by the engine, or of unknown meaning).
/// Sources: doc/yoottower/plugin-schema.md §4 and
/// doc/yoottower/world-and-sp-formats.md §6 (the editor field lists).
///
/// Lookup keys are "<schema>:<path>", where <schema> is the codec name
/// ("TemplateDef", "MemberTable", ...) or, for world records, the resource
/// type ("WORLDDEF", "FBEV", ...; "EVENT" holds the shared event header),
/// and <path> is the dotted Json path with list indices written as "[]"
/// ("variations[].price", "tool.category").
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace editors
{
	struct FieldMeta
	{
		std::string label;                 // caption (empty = the Json key)
		std::string tip;                   // tooltip
		bool hasRange = false;
		int64_t min = 0, max = 0;
		std::vector<std::pair<int64_t, std::string>> options;   // enum dropdown
		std::vector<std::string> tagOptions;                    // 4cc dropdown ("" = none)
		std::vector<std::pair<uint32_t, std::string>> bits;     // bitmask checkboxes
		bool advanced = false;             // unknown / unused by the engine
		bool readOnly = false;
		bool minutes = false;              // minute of day: show hh:mm
		bool boolean = false;              // 0/1 checkbox
		bool multiline = false;            // long text
		bool variableList = false;         // list of scalars that may grow/shrink
		size_t maxItems = 0;               // 0 = unlimited (lists)
	};

	/// Metadata for a field, or nullptr. `schema` and `path` as described above.
	const FieldMeta * findFieldMeta(const std::string & schema, const std::string & path);

	/// Keys that are advanced by name alone: skip*, unread*, unk*, discarded*,
	/// leadByte, sep, padding/hex blobs.
	bool isAdvancedKey(const std::string & key);

	/// Human caption for a key without metadata ("bkgndTileWidth" -> "Bkgnd tile width").
	std::string prettyKey(const std::string & key);
}
