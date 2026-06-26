/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginBitmapLoader.h"
#include "PluginManager.h"
#include "WindowsPEExecutable.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>

#include <cstring>
#include <vector>

using namespace OT;

bool PluginBitmapLoader::rasterizeBitmapResource(const char * data, int length,
                                                 sf::Texture & dst,
                                                 const sf::Color & maskColor)
{
	// YootTower plugins store their bitmaps as raw RT_BITMAP entries:
	// a Windows DIB (BITMAPINFO + bits) without the 14-byte BMP file
	// header. We rebuild the file header in memory, ask SFML to decode
	// it as a normal BMP, optionally apply a transparency mask, then
	// upload to a texture. Lifted from the original YootCondo::loadPEBitmap.
	if (!data || length < 40) return false;

	const uint32_t header_size = *reinterpret_cast<const uint32_t *>(data);
	const uint16_t bit_count   = *reinterpret_cast<const uint16_t *>(data + 14);
	uint32_t clr_used = 0;
	if (length >= 36)
		clr_used = *reinterpret_cast<const uint32_t *>(data + 32);

	uint32_t palette_size = 0;
	if (bit_count <= 8)
		palette_size = (clr_used == 0 ? (1u << bit_count) : clr_used) * 4;

	const uint32_t bfOffBits = 14 + header_size + palette_size;
	const uint32_t bfSize    = 14 + static_cast<uint32_t>(length);

	std::vector<char> bmp(bfSize);
	bmp[0] = 'B'; bmp[1] = 'M';
	std::memcpy(&bmp[2], &bfSize, sizeof(uint32_t));
	std::memset(&bmp[6], 0, 4);                     // reserved words
	std::memcpy(&bmp[10], &bfOffBits, sizeof(uint32_t));
	std::memcpy(&bmp[14], data, static_cast<size_t>(length));

	sf::Image img;
	if (!img.loadFromMemory(bmp.data(), bmp.size())) return false;

	if (maskColor != sf::Color::Transparent)
		img.createMaskFromColor(maskColor);

	return dst.loadFromImage(img);
}

bool PluginBitmapLoader::loadFromPlugin(Plugin & plugin, int resourceId,
                                        sf::Texture & dst,
                                        const sf::Color & maskColor)
{
	// RT_BITMAP is resource type 2 in the Win32 resource type table;
	// plugins store bitmaps under this numeric id rather than a string
	// name, which is why we look up "2" here.
	const WindowsPEExecutable::Resource * res = plugin.getResource("2", resourceId);
	if (!res || !res->data) return false;
	return rasterizeBitmapResource(res->data, res->length, dst, maskColor);
}

bool PluginBitmapLoader::loadFromPEFile(const std::string & path, int resourceId,
                                        sf::Texture & dst,
                                        const sf::Color & maskColor)
{
	// Throwaway Plugin around an arbitrary file path. Used by tests and
	// for one-off loads that bypass the catalog.
	Plugin plugin(Path(path).name(), Path(path));
	if (!plugin.ensureLoaded()) return false;
	return loadFromPlugin(plugin, resourceId, dst, maskColor);
}
