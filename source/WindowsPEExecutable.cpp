#include <fstream>
#include <sys/stat.h>
#include <algorithm>

#include "Application.h"
#include "Logger.h"
#include "WindowsPEExecutable.h"

using namespace OT;
using std::ifstream;
using std::ofstream;
using std::ios;
using std::ios_base;

#pragma pack(push, 1)
struct DOSHeader {
	uint16_t e_magic;
	uint16_t e_cblp;
	uint16_t e_cp;
	uint16_t e_crlc;
	uint16_t e_cparhdr;
	uint16_t e_minalloc;
	uint16_t e_maxalloc;
	uint16_t e_ss;
	uint16_t e_sp;
	uint16_t e_csum;
	uint16_t e_ip;
	uint16_t e_cs;
	uint16_t e_lfarlc;
	uint16_t e_ovno;
	uint16_t e_res[4];
	uint16_t e_oemid;
	uint16_t e_oeminfo;
	uint16_t e_res2[10];
	uint32_t e_lfanew;
};

struct COFFHeader {
	uint16_t Machine;
	uint16_t NumberOfSections;
	uint32_t TimeDateStamp;
	uint32_t PointerToSymbolTable;
	uint32_t NumberOfSymbols;
	uint16_t SizeOfOptionalHeader;
	uint16_t Characteristics;
};

struct DataDirectory {
	uint32_t VirtualAddress;
	uint32_t Size;
};

struct OptionalHeader32 {
	uint16_t Magic;
	uint8_t MajorLinkerVersion;
	uint8_t MinorLinkerVersion;
	uint32_t SizeOfCode;
	uint32_t SizeOfInitData;
	uint32_t SizeOfUninitData;
	uint32_t AddressOfEntryPoint;
	uint32_t BaseOfCode;
	uint32_t BaseOfData;
	uint32_t ImageBase;
	uint32_t SectionAlignment;
	uint32_t FileAlignment;
	uint16_t MajorOSVersion;
	uint16_t MinorOSVersion;
	uint16_t MajorImageVersion;
	uint16_t MinorImageVersion;
	uint16_t MajorSubsystemVersion;
	uint16_t MinorSubsystemVersion;
	uint32_t Win32Version;
	uint32_t SizeOfImage;
	uint32_t SizeOfHeaders;
	uint32_t CheckSum;
	uint16_t Subsystem;
	uint16_t DllCharacteristics;
	uint32_t SizeOfStackReserve;
	uint32_t SizeOfStackCommit;
	uint32_t SizeOfHeapReserve;
	uint32_t SizeOfHeapCommit;
	uint32_t LoaderFlags;
	uint32_t NumberOfRvaAndSizes;
	DataDirectory dataDirectories[16];
};

struct SectionHeader {
	char Name[8];
	uint32_t VirtualSize;
	uint32_t VirtualAddress;
	uint32_t SizeOfRawData;
	uint32_t PointerToRawData;
	uint32_t PointerToRelocations;
	uint32_t PointerToLinenumbers;
	uint16_t NumberOfRelocations;
	uint16_t NumberOfLinenumbers;
	uint32_t Characteristics;
};

struct ResourceDirectory {
	uint32_t Characteristics;
	uint32_t TimeDateStamp;
	uint16_t MajorVersion;
	uint16_t MinorVersion;
	uint16_t NumberOfNamedEntries;
	uint16_t NumberOfIdEntries;
};

struct ResourceDirectoryEntry {
	uint32_t NameOrId;
	uint32_t OffsetToDataOrDirectory;
};

struct ResourceDataEntry {
	uint32_t OffsetToData;
	uint32_t Size;
	uint32_t CodePage;
	uint32_t Reserved;
};
#pragma pack(pop)

static uint32_t rvaToOffset(uint32_t rva, const std::vector<SectionHeader>& sections) {
	for (const auto& sec : sections) {
		uint32_t size = sec.VirtualSize;
		if (size == 0) size = sec.SizeOfRawData;
		if (rva >= sec.VirtualAddress && rva < sec.VirtualAddress + size) {
			return sec.PointerToRawData + (rva - sec.VirtualAddress);
		}
	}
	return 0;
}

static void parseDirectory(ifstream &f, uint32_t dir_offset, uint32_t rsrc_offset, 
                           const std::vector<SectionHeader>& sections, int level, 
                           std::string current_type, int current_id, 
                           WindowsPEExecutable::ResourceTable &resources)
{
	f.seekg(dir_offset, ios::beg);
	ResourceDirectory dir;
	f.read((char*)&dir, sizeof(dir));
	if (f.fail() || f.gcount() < sizeof(dir)) return;

	int total_entries = dir.NumberOfNamedEntries + dir.NumberOfIdEntries;
	if (total_entries <= 0 || total_entries > 10000) return;
	
	std::vector<ResourceDirectoryEntry> entries(total_entries);
	f.read((char*)entries.data(), total_entries * sizeof(ResourceDirectoryEntry));
	if (f.fail() || f.gcount() < total_entries * sizeof(ResourceDirectoryEntry)) return;

	for (const auto& entry : entries) {
		std::string name_or_id_str;
		int name_or_id_int = 0;
		
		if (entry.NameOrId & 0x80000000) {
			uint32_t str_offset = rsrc_offset + (entry.NameOrId & 0x7FFFFFFF);
			size_t backup = f.tellg();
			f.seekg(str_offset, ios::beg);
			uint16_t length = 0;
			f.read((char*)&length, sizeof(length));
			std::string str;
			for (int j = 0; j < length; ++j) {
				uint16_t c = 0;
				f.read((char*)&c, sizeof(c));
				str += (char)c;
			}
			name_or_id_str = str;
			f.seekg(backup, ios::beg);
			if (f.fail()) f.clear();
		} else {
			name_or_id_int = entry.NameOrId & 0x7FFFFFFF;
			name_or_id_str = std::to_string(name_or_id_int);
		}

		if (entry.OffsetToDataOrDirectory & 0x80000000) {
			uint32_t sub_dir_offset = rsrc_offset + (entry.OffsetToDataOrDirectory & 0x7FFFFFFF);
			if (level == 1) {
				parseDirectory(f, sub_dir_offset, rsrc_offset, sections, level + 1, name_or_id_str, current_id, resources);
			} else if (level == 2) {
				parseDirectory(f, sub_dir_offset, rsrc_offset, sections, level + 1, current_type, name_or_id_int, resources);
			} else {
				parseDirectory(f, sub_dir_offset, rsrc_offset, sections, level + 1, current_type, current_id, resources);
			}
		} else {
			uint32_t data_entry_offset = rsrc_offset + (entry.OffsetToDataOrDirectory & 0x7FFFFFFF);
			size_t backup = f.tellg();
			f.seekg(data_entry_offset, ios::beg);
			ResourceDataEntry data_entry;
			f.read((char*)&data_entry, sizeof(data_entry));
			if (f.fail() || f.gcount() < sizeof(data_entry)) {
				f.seekg(backup, ios::beg);
				if (f.fail()) f.clear();
				continue;
			}
			
			uint32_t file_offset = rvaToOffset(data_entry.OffsetToData, sections);
			if (file_offset > 0 && data_entry.Size > 0) {
				WindowsPEExecutable::Resource & r = resources[current_type][current_id];
				r.type = current_type;
				r.id = current_id;
				r.offset = file_offset;
				r.length = data_entry.Size;
				r.data = new char[r.length];
				
				f.seekg(file_offset, ios::beg);
				f.read(r.data, r.length);
			}
			
			f.seekg(backup, ios::beg);
			if (f.fail()) f.clear();
		}
	}
}

bool WindowsPEExecutable::loadPE(Path path)
{
	ifstream f(path.c_str(), ios_base::in | ios_base::binary);
	if (!f.is_open())
		return false;

	DOSHeader dosHeader;
	f.read((char*)&dosHeader, sizeof(dosHeader));
	if (f.fail() || f.gcount() < sizeof(dosHeader)) return false;

	if (dosHeader.e_magic != 0x5A4D) {
		return false;
	}

	f.seekg(dosHeader.e_lfanew, ios::beg);
	uint32_t pe_sig = 0;
	f.read((char*)&pe_sig, sizeof(pe_sig));
	if (f.fail() || pe_sig != 0x00004550) {
		return false;
	}

	COFFHeader coffHeader;
	f.read((char*)&coffHeader, sizeof(coffHeader));
	if (f.fail()) return false;

	OptionalHeader32 optHeader;
	if (coffHeader.SizeOfOptionalHeader < sizeof(OptionalHeader32)) {
		return false;
	}
	f.read((char*)&optHeader, sizeof(optHeader));
	if (f.fail()) return false;

	if (optHeader.Magic != 0x10b) {
		return false;
	}

	uint32_t section_table_offset = dosHeader.e_lfanew + 4 + sizeof(COFFHeader) + coffHeader.SizeOfOptionalHeader;
	f.seekg(section_table_offset, ios::beg);
	std::vector<SectionHeader> sections(coffHeader.NumberOfSections);
	f.read((char*)sections.data(), coffHeader.NumberOfSections * sizeof(SectionHeader));
	if (f.fail()) return false;

	if (optHeader.NumberOfRvaAndSizes <= 2) {
		return true;
	}

	DataDirectory rsrc_dir_info = optHeader.dataDirectories[2];
	if (rsrc_dir_info.VirtualAddress == 0 || rsrc_dir_info.Size == 0) {
		return true;
	}

	uint32_t rsrc_offset = rvaToOffset(rsrc_dir_info.VirtualAddress, sections);
	if (rsrc_offset == 0) {
		return false;
	}

	parseDirectory(f, rsrc_offset, rsrc_offset, sections, 1, "", 0, resources);
	return true;
}

bool WindowsPEExecutable::loadSP(Path path)
{
	ifstream f(path.c_str(), ios_base::in | ios_base::binary);
	if (!f.is_open())
		return false;

	char magic[2];
	f.read(magic, 2);
	if (f.fail() || magic[0] != 'S' || magic[1] != 'P')
		return false;

	uint32_t count = 0;
	f.read((char*)&count, sizeof(count));
	if (f.fail())
		return false;

#pragma pack(push, 1)
	struct SPEntry {
		char sig[4];
		uint32_t id;
		uint32_t offset;
		uint32_t size;
	};
#pragma pack(pop)

	std::vector<SPEntry> entries(count);
	f.read((char*)entries.data(), count * sizeof(SPEntry));
	if (f.fail() || f.gcount() < count * sizeof(SPEntry))
		return false;

	for (const auto& entry : entries) {
		std::string type_name;
		if (entry.sig[0] == 0 && entry.sig[1] == 0 && entry.sig[2] == 0 && entry.sig[3] == 0) {
			type_name = "RAW";
		} else {
			type_name = "";
			for (int j = 3; j >= 0; --j) {
				if (entry.sig[j] != 0 && entry.sig[j] != ' ') {
					type_name += entry.sig[j];
				}
			}
		}

		if (entry.size > 0 && entry.offset > 0) {
			Resource & r = resources[type_name][entry.id];
			r.type = type_name;
			r.id = entry.id;
			r.offset = entry.offset;
			r.length = entry.size;
			r.data = new char[r.length];

			f.seekg(entry.offset, ios::beg);
			f.read(r.data, r.length);
		}
	}
	return true;
}

bool WindowsPEExecutable::load(Path path)
{
	ifstream f(path.c_str(), ios_base::in | ios_base::binary);
	if (!f.is_open())
		return false;

	char magic[2];
	f.read(magic, 2);
	if (f.fail() || f.gcount() < 2)
		return false;

	f.close();

	if (magic[0] == 'M' && magic[1] == 'Z') {
		return loadPE(path);
	} else if (magic[0] == 'S' && magic[1] == 'P') {
		return loadSP(path);
	}
	return false;
}

void WindowsPEExecutable::dump(Path path)
{
	struct stat st;
	if (stat(path, &st) != 0) {
		if (mkdir(path, 0777) != 0) {
			return;
		}
	}
	
	for (auto& t : resources) {
		Path dir = path.down(t.first);
		if (stat(dir, &st) != 0) {
			if (mkdir(dir, 0777) != 0) {
				return;
			}
		} 
		
		for (auto& r : t.second) {
			char temp[16];
			snprintf(temp, 16, "%x.bin", r.second.id);
			Path p = dir.down(temp);
			
			ofstream f(p.c_str(), ios::binary);
			if (f.is_open() && r.second.data && r.second.length > 0) {
				f.write(r.second.data, r.second.length);
			}
			f.close();
		}
	}
}
