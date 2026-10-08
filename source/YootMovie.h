/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// Soundtrack of a YootTower theater / event-hall clip (Movie/MV_*.MOV and
/// HE_*.MOV, named by MovieDef::movieFile and HallEventDef::movieFile).
///
/// The shipped clips are QuickTime movies with a 208x117 (or 208x60)
/// Cinepak video track and an uncompressed PCM sound track ('raw ' u8,
/// 11025/11127 Hz, mono; MV_MEKASIA is stereo). OpenSky does not decode
/// Cinepak; it shows the plugin's still (DIB 129) and plays the sound track,
/// which needs only the QuickTime atom tree: moov/trak/mdia/hdlr 'soun' ->
/// minf/stbl/{stsd, stsc, stco/co64} gives the PCM chunks.
///
/// Pure module: no SFML dependency, linked into OpenSkyTests.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OT
{
	struct YootMovieSound
	{
		unsigned channels = 0;
		unsigned sampleRate = 0;          ///< Hz (16.16 fixed point, integer part)
		std::vector<int16_t> samples;     ///< interleaved, signed 16-bit
	};

	/// Extracts the first sound track of a QuickTime movie held in memory.
	/// Supports 'raw ' (unsigned 8-bit) and 'twos' (signed 8/16-bit
	/// big-endian) PCM. Returns false when there is no such track.
	bool decodeQuickTimeSound(const char * data, size_t length, YootMovieSound & out);
	/// Reads `path` and decodes it.
	bool loadQuickTimeSound(const std::string & path, YootMovieSound & out);
}
