/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginBitmapLoader.h"
#include "AbstractPlugin.h"
#include "PluginManager.h"
#include "WindowsPEExecutable.h"

#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/Image.hpp>

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

bool PluginBitmapLoader::decodeBitmapResource(const char * data, int length,
                                              sf::Image & dst)
{
	// Same DIB reconstruction as rasterizeBitmapResource, but stops at the
	// decoded sf::Image (no GPU upload, no mask applied). The editor calls
	// this to obtain raw pixels for PNG export / mask discovery.
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
	std::memset(&bmp[6], 0, 4);
	std::memcpy(&bmp[10], &bfOffBits, sizeof(uint32_t));
	std::memcpy(&bmp[14], data, static_cast<size_t>(length));

	return dst.loadFromMemory(bmp.data(), bmp.size());
}

void PluginBitmapLoader::downscaleImage(const sf::Image & src, sf::Image & dst,
                                        unsigned int targetW, unsigned int targetH)
{
	const sf::Vector2u srcSize = src.getSize();
	// Already at or below target: copy through unchanged.
	if (srcSize.x <= targetW || srcSize.y <= targetH)
	{
		if (&src != &dst) dst = src;
		return;
	}

	dst.resize({targetW, targetH});
	// Block size per output pixel. Each source block may have a different
	// covered count at the right/bottom edge, so we average per-block.
	for (unsigned int oy = 0; oy < targetH; ++oy)
	{
		const unsigned int y0 = (oy * srcSize.y) / targetH;
		const unsigned int y1 = ((oy + 1) * srcSize.y) / targetH;
		for (unsigned int ox = 0; ox < targetW; ++ox)
		{
			const unsigned int x0 = (ox * srcSize.x) / targetW;
			const unsigned int x1 = ((ox + 1) * srcSize.x) / targetW;
			// Alpha-weighted average so colour-keyed (binary alpha) sprites
			// downscale without dark fringes: fully-transparent pixels
			// contribute no colour, and the output alpha is the coverage.
			unsigned long long r = 0, g = 0, b = 0, a = 0, cover = 0;
			for (unsigned int y = y0; y < y1; ++y)
			{
				for (unsigned int x = x0; x < x1; ++x)
				{
					const sf::Color c = src.getPixel({x, y});
					const unsigned int wa = c.a;
					r += c.r * wa;
					g += c.g * wa;
					b += c.b * wa;
					a += wa;
					++cover;
				}
			}
			if (cover == 0) { dst.setPixel({ox, oy}, sf::Color::Transparent); continue; }
			const unsigned int outA = static_cast<unsigned int>(a / cover);
			const unsigned int div  = (outA == 0) ? 1 : outA;
			dst.setPixel({ox, oy}, sf::Color(
			    static_cast<uint8_t>(r / div),
			    static_cast<uint8_t>(g / div),
			    static_cast<uint8_t>(b / div),
			    static_cast<uint8_t>(outA)));
		}
	}
}

bool PluginBitmapLoader::loadFromPlugin(AbstractPlugin & plugin, int resourceId,
                                        sf::Texture & dst,
                                        const sf::Color & maskColor)
{
	// RT_BITMAP is resource type 2 in the Win32 resource type table;
	// plugins store bitmaps under this numeric id rather than a string
	// name, which is why we look up "2" here.
	const WindowsPEExecutable::Resource * res = plugin.getResource("2", resourceId);
	// SP sub-plugins (MV_*, Pl*, ads) keep their DIBs as type-0 ("RAW") entries.
	if (!res) res = plugin.getResource("RAW", resourceId);
	if (!res || !res->data) return false;

	// The backing store determines the byte format. PE plugins serve a
	// raw DIB (reconstruct a BMP header); folder plugins serve PNG bytes
	// (decode directly). We sniff: a PNG starts with the 8-byte signature
	// 0x89 'P' 'N' 'G' \r \n 0x1A \n. Anything else is treated as a DIB.
	static const unsigned char kPngSig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	const bool isPng = (res->length >= 8 &&
	                    std::memcmp(res->data, kPngSig, 8) == 0);

	sf::Image img;
	bool decoded = false;
	if (isPng)
		decoded = img.loadFromMemory(res->data, static_cast<std::size_t>(res->length));
	else
		decoded = decodeBitmapResource(res->data, res->length, img);
	if (!decoded) return false;

	if (maskColor != sf::Color::Transparent)
		img.createMaskFromColor(maskColor);

	// OpenTower folder plugins declare a downscale target so the ~3x art
	// lands on the engine's cell grid. PE plugins have no target and the
	// image is uploaded as-is.
	unsigned int tw = 0, th = 0;
	if (plugin.downscaleTarget(tw, th) && tw > 0 && th > 0)
	{
		sf::Image downscaled;
		downscaleImage(img, downscaled, tw, th);
		return dst.loadFromImage(downscaled);
	}
	return dst.loadFromImage(img);
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
