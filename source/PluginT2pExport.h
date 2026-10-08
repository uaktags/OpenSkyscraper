/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Writes real YootTower plugins (.t2p) from edited resources.
///
///  * PE plugins (tenants, movers, worlds, ...): the original DLL keeps its
///    code; only the .rsrc section is rebuilt from the resource list. The
///    rebuild reproduces the MS linker layout (directory tables breadth
///    first, then data entries, then the name strings, then the payloads
///    8-byte aligned) and keeps the original order of the name strings and
///    of the payloads, so an unedited export is byte-identical to the input.
///    Sections after .rsrc (.reloc) and the trailing debug data are moved
///    when .rsrc changes size; the PE headers (section table, data
///    directories, SizeOfImage, SizeOfInitializedData, debug directory file
///    pointers and, when the input had one, the checksum) are fixed up.
///  * SP plugins (movies, hall events, ads): the "SP" container is
///    re-encoded in directory order (encodeSpContainer).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	struct OspPlugin;

	/// One resource to place in the rebuilt .rsrc. `type` uses the
	/// WindowsPEExecutable keys: a decimal string for numeric types ("2" =
	/// RT_BITMAP) or the type name ("TNDF").
	struct PeResourceData
	{
		std::string type;
		int id = 0;
		std::string data;
	};

	/// Rebuilds the resource section of the PE image `pe` so it holds exactly
	/// `resources` (resources absent from the list are removed, new ones are
	/// added with language 0). Fails with `error` on an unsupported image
	/// (not PE32, .rsrc not starting a section, named resource ids).
	bool rebuildPeResources(const std::string & pe, const std::vector<PeResourceData> & resources,
	                        std::string & out, std::string * error = nullptr);

	/// PE image checksum (the IMAGEHLP CheckSumMappedFile algorithm).
	uint32_t computePeChecksum(const std::string & pe);

	/// Encodes every resource of `plugin` and builds the .t2p bytes: the PE
	/// rebuild over `plugin.peImage`, or the SP container. Needs a plugin
	/// converted from a .t2p (OSP v3); v1/v2 and scratch plugins have no code
	/// image to export into.
	bool exportOspToT2p(const OspPlugin & plugin, std::string & out, std::string * error = nullptr);
	/// exportOspToT2p + write to `path`.
	bool writeT2pFile(const OspPlugin & plugin, const std::string & path, std::string * error = nullptr);
}
