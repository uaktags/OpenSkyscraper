/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Json views of every typed YootTower resource (PluginResourceCodec.h).
///
/// The OSP v3 format (OspPlugin.h) writes each decoded resource as one of
/// these Json documents and the Plugin Editor edits resources through them,
/// so the file and the editor always agree on field names. Conversions are
/// lossless: fromJson(toJson(x)) encodes to the same bytes as x, including
/// the `skip*` / `unread*` / `unk*` fields the engine ignores.
///
/// Conventions:
///  * keys are the struct field names (engine names without the `m`);
///  * plugin strings are code-page bytes mapped through Latin-1;
///  * 4ccs are written in Mac order ("BURG", "BoPI"), "" for 0, or as the
///    little-endian number when not printable;
///  * TemplateDef flattens the per-variation ToolDef and EquipDef records
///    into one "variations" list, so adding a row adds both halves;
///  * lists of records are wrapped in an object ({"entries": [...]}), so
///    every resource document is a Json object.
#pragma once
#include "PluginResourceCodec.h"
#include "ResourceJson.h"

#include <string>

namespace OT
{
	/// Typed resource -> Json. Raw/Text resources have no Json form (null).
	Json resourceToJson(const DecodedResource & res);

	/// Json -> typed resource. `res.type`, `res.id` and `res.codec` select
	/// the layout; on success `res.value` is replaced. On failure `error`
	/// names the offending field ("tool.width: expected a 32-bit integer").
	bool resourceFromJson(const Json & j, DecodedResource & res, std::string * error = nullptr);

	/// 4cc <-> Json helpers shared with the editor.
	Json fourccToJson(const FourCC & f);
	bool fourccFromJson(const Json & j, FourCC & out);

	/// UTF-16 <-> UTF-8 that also carries unpaired surrogates (WTF-8).
	std::string utf16ToWtf8(const std::u16string & s);
	bool wtf8ToUtf16(const std::string & s, std::u16string & out);
}
