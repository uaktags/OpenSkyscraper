#pragma once
#include "Path.h"
#include <map>
#include <string>
#include <vector>
#include <cstring>

namespace OT
{
	class WindowsPEExecutable
	{
	public:
		struct Resource {
			std::string type;
			int id;
			int offset;
			int length;
			char * data;
			Resource() : data(nullptr), length(0), offset(0), id(0) {}
			~Resource() { delete[] data; }
			Resource(const Resource& other) : type(other.type), id(other.id), offset(other.offset), length(other.length), data(nullptr) {
				if (other.data && other.length > 0) {
					data = new char[other.length];
					std::memcpy(data, other.data, other.length);
				}
			}
			Resource& operator=(const Resource& other) {
				if (this != &other) {
					delete[] data;
					type = other.type;
					id = other.id;
					offset = other.offset;
					length = other.length;
					if (other.data && other.length > 0) {
						data = new char[other.length];
						std::memcpy(data, other.data, other.length);
					} else {
						data = nullptr;
					}
				}
				return *this;
			}
		};
		typedef std::map<int, Resource> Resources;
		typedef std::map<std::string, Resources> ResourceTable;
		
		ResourceTable resources;
		
		bool load(Path path);
		bool loadPE(Path path);
		bool loadSP(Path path);
		void dump(Path path);
	};
}
