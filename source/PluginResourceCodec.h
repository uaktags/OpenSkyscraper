/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Generic per-resource-type dispatcher for YootTower plugins: decode any
/// resource of a PE or SP plugin into its typed structure and encode it back
/// to the identical bytes.
///
/// Coverage (doc/yoottower/plugin-schema.md §2, world-and-sp-formats.md §3):
///   ATTR, DESC                       PluginResources.h
///   TNDF/MVDF/OODF, TMDF, DPDF, PLDF,
///   PADF, SLDF, PPDF, WODF,
///   STRTABLE/STRTBL                  PluginDefs.h
///   MODF, HEDF, ADDF, SP PPDF        SubPluginDefs.h
///   RT_STRING                        StringBlock (16 UTF-16 strings)
///   RT_VERSION                       VersionInfo (VS_VERSIONINFO node tree)
///   OBJMAP, T2DLG, TENANTDEF         Text (verbatim; OBJMAP has its own
///                                    parser in PluginObjmap.h)
///   everything else                  Raw: RT_BITMAP, WAVE, SP DIBs, and the
///                                    world resources (WORLDDEF, WATR, BINF,
///                                    FINF, PTDF, PALETTE, T2EV, *EV, PODF,
///                                    PGDF, PZDF, SPDF, CODF, VIPL), which
///                                    are decoded by YootWorld.
///
/// A typed decoder that rejects a resource (e.g. trailing bytes) falls back to
/// Raw, so `decodeResource` never loses data; `codec` says which path won.
#pragma once
#include "PluginDefs.h"
#include "PluginResources.h"
#include "SubPluginDefs.h"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace OT
{
	/// RT_STRING block: up to 16 counted UTF-16 strings (Win32 string table).
	struct StringBlock
	{
		std::vector<std::u16string> strings;
		std::string trailing;         // bytes after the 16th string (normally empty)
	};

	bool decodeStringBlock(const char * data, size_t length, StringBlock & out);
	bool encodeStringBlock(const StringBlock & block, std::string & out);

	/// One VS_VERSIONINFO-style node (VS_VERSIONINFO, StringFileInfo, String,
	/// Var, ...). Alignment padding that is not all zero is preserved so the
	/// tree re-encodes byte for byte.
	struct VersionNode
	{
		uint16_t length = 0;          // wLength as stored
		uint16_t valueLength = 0;     // wValueLength (UTF-16 units for text values)
		uint16_t type = 0;            // wType: 1 = text, 0 = binary
		std::u16string key;
		std::string value;            // raw value bytes (UTF-16LE for type 1)
		std::vector<VersionNode> children;
		std::string pad1;             // non-zero padding after the key (else empty)
		std::string pad2;             // non-zero padding after the value
		std::string padAfter;         // non-zero padding after this node inside its parent
		std::string extra;            // unparsed bytes at the end of the node

		/// Key as ASCII/UTF-8.
		std::string keyString() const;
		/// Type-1 value as UTF-8 (without the trailing NUL).
		std::string textValue() const;
		/// Depth-first search for a child with `key`.
		const VersionNode * find(const std::string & key) const;
	};

	/// RT_VERSION resource.
	struct VersionInfo
	{
		VersionNode root;
		std::string trailing;
	};

	/// Decodes and verifies that the tree re-encodes identically.
	bool decodeVersionInfo(const char * data, size_t length, VersionInfo & out);
	bool encodeVersionInfo(const VersionInfo & info, std::string & out);

	enum class ResourceCodec
	{
		Raw,                  // kept as bytes (`bytes`)
		Text,                 // kept as text (`bytes`)
		Attr,
		Desc,
		TemplateDef,          // TNDF / MVDF / OODF
		MemberTable,          // TMDF
		DayParam,             // DPDF
		PlaceParam,           // PLDF
		PeopleAnime,          // PADF
		Silhouettes,          // SLDF
		People,               // PPDF (PE)
		Words,                // WODF
		StringTable,          // STRTABLE / STRTBL
		Movie,                // MODF (SP)
		HallEvent,            // HEDF (SP)
		Advertise,            // ADDF (SP)
		SubPluginPeopleParam, // PPDF (SP)
		StringBlock,          // RT_STRING
		VersionInfo,          // RT_VERSION
	};

	/// Human-readable codec name ("TemplateDef", "Raw", ...).
	const char * resourceCodecName(ResourceCodec c);

	/// Which container a resource came from. Only matters where the two
	/// formats reuse a tag with different layouts (PPDF).
	enum class ResourceContainer { PE, SP };

	/// A decoded resource. `value` holds the typed struct for every codec
	/// except Raw/Text, which use `bytes`.
	struct DecodedResource
	{
		std::string type;             // resource type key as stored ("TNDF", "2", ...)
		int id = 0;
		ResourceCodec codec = ResourceCodec::Raw;
		std::variant<std::monostate, PluginAttr, PluginDesc, TemplateDef, MemberTableDef,
		             DayParamDef, PlaceParamDef, PeopleAnimeDef, SilhouetteList, PeopleList,
		             WordList, StringTableDef, MovieDef, HallEventDef, AdvertiseDef,
		             SubPluginPeopleParamDef, StringBlock, VersionInfo> value;
		std::string bytes;            // Raw / Text payload
	};

	/// Maps the numeric PE type keys to their RT_* names ("2" -> "RT_BITMAP");
	/// named types are returned unchanged.
	std::string resourceTypeDisplayName(const std::string & type);

	/// The codec `decodeResource` will try for a type key (before fallback).
	ResourceCodec codecForResourceType(const std::string & type, ResourceContainer container);

	/// Decodes one resource. Never fails: an unrecognised or rejected payload
	/// is kept Raw.
	DecodedResource decodeResource(const std::string & type, int id, const char * data, size_t length,
	                               ResourceContainer container = ResourceContainer::PE);

	/// Encodes a decoded resource back to bytes. Returns false only when the
	/// typed value cannot be represented (e.g. a string over 255 bytes or an
	/// EquipDef/ToolDef variation count mismatch).
	bool encodeResource(const DecodedResource & res, std::string & out);
}
