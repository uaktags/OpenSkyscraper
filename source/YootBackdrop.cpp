/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "YootBackdrop.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace OT
{
	namespace
	{
		uint32_t le32(const unsigned char * p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
		uint16_t le16(const unsigned char * p) { return uint16_t(p[0] | (p[1] << 8)); }
		uint16_t be16(const unsigned char * p) { return uint16_t((p[0] << 8) | p[1]); }

		/// CResFile text-mode integer tokenizer (CResFile::Token 0x10011290):
		/// separators are space/tab/CR/LF/comma, "(...)" is a comment.
		bool readTextInts(const char * data, int length, std::vector<long long> & out)
		{
			const char * p = data;
			const char * end = data + length;
			while (p < end)
			{
				const char c = *p;
				if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',' || c == '\0') { ++p; continue; }
				if (c == '(')
				{
					while (p < end && *p != ')') ++p;
					if (p < end) ++p;
					continue;
				}
				bool neg = false;
				if (c == '-') { neg = true; ++p; }
				if (p >= end || !std::isdigit(uint8_t(*p))) return false;
				long long v = 0;
				while (p < end && std::isdigit(uint8_t(*p))) { v = v * 10 + (*p - '0'); ++p; }
				out.push_back(neg ? -v : v);
			}
			return true;
		}

		int floorDiv(int a, int b)
		{
			int q = a / b;
			if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
			return q;
		}

		uint8_t clampByte(int v) { return uint8_t(v < 0 ? 0 : (v > 255 ? 255 : v)); }

		// T2DLL .rdata constants used by T2DayLightFilter (0x10081cb8..0x10081d88).
		const double kTwoPiOver1000 = 0.006283184000000001;
		const double kHalfPi = 1.570796;
		const double kPi = 3.141592;
		const double kDegToRad = 0.01745328888888889;
		const double kInv255 = 0.00392156862745098;
	}

	// ------------------------------------------------------------------
	// DIB

	bool decodeYootIndexedDib(const char * data, int length, YootIndexedImage & out)
	{
		if (!data || length < 40) return false;
		const unsigned char * d = reinterpret_cast<const unsigned char *>(data);
		const uint32_t headerSize = le32(d);
		const int32_t w = int32_t(le32(d + 4));
		const int32_t h = int32_t(le32(d + 8));
		const uint16_t bpp = le16(d + 14);
		const uint32_t compression = le32(d + 16);
		uint32_t colours = le32(d + 32);
		if (bpp != 8 || compression != 0 || w <= 0 || h == 0 || headerSize < 40) return false;
		if (colours == 0 || colours > 256) colours = 256;
		const int height = h < 0 ? -h : h;
		const std::size_t stride = (std::size_t(w) + 3) & ~std::size_t(3);
		const std::size_t bitsOffset = headerSize + std::size_t(colours) * 4;
		if (bitsOffset + stride * height > std::size_t(length)) return false;

		out.width = w;
		out.height = height;
		out.palette.assign(256, YootRgb());
		for (uint32_t i = 0; i < colours; ++i)
		{
			const unsigned char * q = d + headerSize + i * 4; // RGBQUAD: b, g, r, reserved
			out.palette[i] = YootRgb(q[2], q[1], q[0]);
		}
		out.pixels.resize(std::size_t(w) * height);
		for (int y = 0; y < height; ++y)
		{
			const int srcRow = h > 0 ? (height - 1 - y) : y; // positive height = bottom-up
			std::memcpy(&out.pixels[std::size_t(y) * w], d + bitsOffset + stride * srcRow, std::size_t(w));
		}
		return true;
	}

	// ------------------------------------------------------------------
	// BINF

	int YootBinf::tile(int row, int col) const
	{
		if (rows <= 0 || cols <= 0 || cells.empty()) return 0;
		if (row < 0) row = 0;
		if (row >= rows) row = rows - 1;
		if (col < 0) col = 0;
		if (col >= cols) col = cols - 1;
		return int(cells[std::size_t(col) * rows + row] & 0xffff);
	}

	bool parseYootBinf(const char * data, int length, YootBinf & out)
	{
		std::vector<long long> v;
		if (!data || !readTextInts(data, length, v) || v.size() < 3) return false;
		const long long rows = v[0], cols = v[1];
		if (rows <= 0 || cols <= 0 || v.size() < 3 + std::size_t(rows * cols)) return false;
		out.rows = int(rows);
		out.cols = int(cols);
		out.unused = int(v[2]);
		out.cells.resize(std::size_t(rows * cols));
		for (std::size_t k = 0; k < out.cells.size(); ++k)
			out.cells[k] = uint32_t(v[3 + k]);
		return true;
	}

	YootBackdropGeometry::Rect YootBackdropGeometry::cellRect(int row, int col) const
	{
		Rect r;
		r.x = (col * cellUnits - unitOrigin) * kUnitPixels;
		r.y = (row * cellRows - groundLine) * kRowPixels;
		r.w = cellUnits * kUnitPixels;
		r.h = cellRows * kRowPixels;
		return r;
	}

	int YootBackdropGeometry::cellRowAt(double pixelY) const
	{
		const int row = int(std::floor(pixelY / kRowPixels)) + groundLine;
		return floorDiv(row, cellRows);
	}

	int YootBackdropGeometry::cellColAt(double pixelX) const
	{
		const int unit = int(std::floor(pixelX / kUnitPixels)) + unitOrigin;
		return floorDiv(unit, cellUnits);
	}

	YootBackdropGeometry::Rect YootBackdropGeometry::worldRect() const
	{
		Rect r;
		r.x = -unitOrigin * kUnitPixels;
		r.y = -groundLine * kRowPixels;
		r.w = width * kUnitPixels;
		r.h = height * kRowPixels;
		return r;
	}

	// ------------------------------------------------------------------
	// PTDF / PALETTE

	bool parseYootPtdf(const char * data, int length, YootPaletteAnimeDef & out)
	{
		// Each line is "(mName)<tabs>value"; the comment holds the name, so
		// the integer tokens arrive in the constructor's read order.
		std::vector<long long> v;
		if (!data || !readTextInts(data, length, v) || v.size() < 8) return false;
		out.numTowerColor = int(v[0]);
		out.numAnimColor = int(v[1]);
		out.animStartIndex = int(v[2]);
		out.numRefractColor = int(v[3]);
		out.numSkyColor = int(v[4]);
		out.numSEKind = int(v[5]);
		out.numSEColor = int(v[6]);
		out.seInterval = int(v[7]);
		return true;
	}

	bool parseYootPalette(const char * data, int length, std::vector<YootRgb> & out)
	{
		if (!data || length < 2) return false;
		const unsigned char * d = reinterpret_cast<const unsigned char *>(data);
		out.clear();
		if (length == 0x300)
		{
			for (int i = 0; i < 256; ++i)
				out.push_back(YootRgb(d[i * 3], d[i * 3 + 1], d[i * 3 + 2]));
			return true;
		}
		if (d[0] == 0 && d[1] == 0)
		{
			// Mac clut: u32 seed, u16 flags, u16 ctSize, then {index, r, g, b}.
			if (length < 8) return false;
			const int ctSize = be16(d + 6);
			const int n = (length - 8) / 8;
			out.assign(256, YootRgb());
			for (int k = 0; k < n && k < ctSize; ++k)
			{
				const unsigned char * e = d + 8 + k * 8;
				out[be16(e) & 0xff] = YootRgb(e[2], e[4], e[6]);
			}
			return true;
		}
		// Mac pltt: u16 count, 14 reserved bytes, 16-byte {r, g, b, ...} entries.
		const int count = be16(d);
		if (16 + 16 * count > length) return false;
		for (int k = 0; k < count; ++k)
		{
			const unsigned char * e = d + 16 + k * 16;
			out.push_back(YootRgb(e[0], e[2], e[4]));
		}
		return true;
	}

	// ------------------------------------------------------------------
	// T2DayLightFilter

	int yootDayLightAngle(int day, int timeOf1000)
	{
		// SetTime 0x1001cd00. Day length D = 500 - 8*sin(day*2pi/1000) (in
		// 1/1000 day), night half N = (1000-D)/2: before sunrise the angle
		// rises from -90 (cos), during the day it arcs 0..90..0 (sin), after
		// sunset it falls back towards -90.
		const double t = double(timeOf1000);
		const double dayLen = 500.0 - std::sin(double(day) * kTwoPiOver1000) * -8.0;
		const double night = (1000.0 - dayLen) * 0.5;
		double v;
		if (t < night)
			v = std::cos((t / night) * kHalfPi) * -90.0;
		else if (t < 1000.0 - night)
			v = std::sin(((t - night) / dayLen) * kPi) * 90.0;
		else
			v = std::sin(((t - (1000.0 - night)) / night) * kHalfPi) * -90.0;
		// Round half away from zero, then _ftol truncation.
		int angle = int(v < 0.0 ? v - 0.5 : v + 0.5);
		if (angle > 90) angle = 90;
		if (angle < -90) angle = -90;
		while (angle < 0) angle += 360;
		while (angle >= 360) angle -= 360;
		return angle;
	}

	int yootDayLightAngleForMinute(int minuteOfDay)
	{
		// T2PaletteAnime::AdjustSkyState: SetTime(0, minute*1000/1440).
		const unsigned minute = unsigned(((minuteOfDay % 1440) + 1440) % 1440);
		return yootDayLightAngle(0, int(minute * 1000u / 1440u));
	}

	YootRgb yootDayLightFilter(int angle)
	{
		// CalcDayLightColor 0x1001cec0.
		const double s = std::sin(double(angle) * kDegToRad);
		double t = (s - -1.0) * 0.5;
		if (t < 0.0) t = 0.0;
		double u = s * 1.1;
		if (u > 1.0) u = 1.0;
		else if (u < 0.1) u = 0.1;
		const double w = 1.0 - (1.0 - u) * 0.8;
		const double a = w * t;            // green base
		const double b = t - -0.4;         // red base
		const double c = u * t - -0.4;     // blue
		double x = b * 0.35 + (c - -1.0) * 0.2 + (a - -0.4) * 0.45;
		if (x > 1.0) x = 1.0;
		const double y = 1.0 - (1.0 - x) * 0.7;
		auto clamp01 = [](double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); };
		const double red = clamp01(x * b);
		const double green = clamp01(y * (a - -0.4));
		const double blue = clamp01(c);
		return YootRgb(uint8_t(int(red * 255.0)), uint8_t(int(green * 255.0)), uint8_t(int(blue * 255.0)));
	}

	YootRgb yootSkyColor(const YootRgb & filter, const YootRgb & base)
	{
		// SkyColor 0x1001d110: out = f * (k*base/255 + (1-k)), k = (.45, .6, .6).
		const double rf = filter.r * kInv255, gf = filter.g * kInv255, bf = filter.b * kInv255;
		const double r = (double(base.r) * rf * kInv255) * 0.45 + rf * 0.55;
		const double g = (double(base.g) * gf * kInv255) * 0.6 + gf * 0.4;
		const double b = (double(base.b) * bf * kInv255) * 0.6 + bf * 0.4;
		return YootRgb(uint8_t(int(r * 255.0)), uint8_t(int(g * 255.0)), uint8_t(int(b * 255.0)));
	}

	YootRgb yootRefractionColor(const YootRgb & filter, const YootRgb & base)
	{
		// RefractionColor 0x1001ce40.
		return YootRgb(uint8_t(unsigned(filter.r) * base.r / 255u),
		               uint8_t(unsigned(filter.g) * base.g / 255u),
		               uint8_t(unsigned(filter.b) * base.b / 255u));
	}

	// ------------------------------------------------------------------
	// Tokyo sky

	YootSkyState yootTokyoSkyState(int month, int minuteOfDay, bool rainy)
	{
		// AdjustSkyStateProc 0x10014e90 -> FUN_10014d40(rise, set).
		int rise = 5, set = 17;
		switch (((month % 12) + 12) % 12 / 3)
		{
			case 1: rise = 4; set = 18; break;
			case 3: rise = 6; set = 16; break;
			default: break;
		}
		const int minute = ((minuteOfDay % 1440) + 1440) % 1440;
		const int hour = minute / 60;
		const int inHour = minute % 60;
		const int count = 60 - inHour;
		// FUN_10014c80: Dec mornings count as a snowy night (state 4).
		auto snowNight = [&]() {
			if ((month % 12) / 3 != 3) return false;
			return hour < 12 ? (month % 12) == 11 : (month % 12) == 0;
		};
		YootSkyState s;
		if (hour < rise)
			s.state = snowNight() ? 4 : 2;
		else if (hour < rise + 1) { s.state = 7; s.count = count; }
		else if (hour < rise + 2) { s.state = 8; s.count = count; }
		else if (hour < set)
		{
			// FUN_10014cd0: the rain front arrives at 8:00 and leaves by 16:00.
			if (rainy && hour >= 8 && hour < 9) { s.state = 9; s.count = count; }
			else if (rainy && hour >= 9 && hour < 15) s.state = 3;
			else if (rainy && hour >= 15 && hour < 16) { s.state = 10; s.count = count; }
			else s.state = 1;
		}
		else if (hour < set + 1) { s.state = 5; s.count = count; }
		else if (hour < set + 2) { s.state = 6; s.count = count; }
		else s.state = snowNight() ? 4 : 2;
		return s;
	}

	std::vector<YootRgb> yootTokyoSkyColors(const std::vector<YootRgb> & daylight, const YootSkyState & s)
	{
		auto entry = [&](int k) { return k >= 0 && k < int(daylight.size()) ? daylight[k] : YootRgb(); };
		YootRgb block[8];
		int base = -1;
		switch (s.state)
		{
			case 1: base = 8; break;
			case 2: case 4: base = 24; break;
			case 3: base = 32; break;
			default: break;
		}
		if (base >= 0)
		{
			for (int k = 0; k < 8; ++k) block[k] = entry(base + k);
		}
		else
		{
			// FUN_10014f10: blend `from` into `to`. The original divides the
			// signed difference by 60 BEFORE multiplying by the remaining
			// minutes (byte arithmetic), so small steps snap to `to`.
			int from = 8, to = 8;
			switch (s.state)
			{
				case 5: from = 8;  to = 16; break;
				case 6: from = 16; to = 24; break;
				case 7: from = 24; to = 16; break;
				case 8: from = 16; to = 8;  break;
				case 9: from = 8;  to = 32; break;
				case 10: from = 32; to = 8; break;
				default: break;
			}
			auto mix = [&](uint8_t f, uint8_t t) {
				const int diff = int(f) - int(t);
				return uint8_t(int8_t(diff / 60) * int8_t(s.count) + t);
			};
			for (int k = 0; k < 8; ++k)
			{
				const YootRgb f = entry(from + k), t = entry(to + k);
				block[k] = YootRgb(mix(f.r, t.r), mix(f.g, t.g), mix(f.b, t.b));
			}
		}
		std::vector<YootRgb> out(28);
		for (int k = 0; k < 8; ++k) out[k] = block[k];
		for (int k = 8; k < 28; ++k) out[k] = block[(k - 8) % 4];
		return out;
	}

	// ------------------------------------------------------------------
	// Animated range

	void yootApplyAnimatedPalette(YootSkyModel model, const YootPaletteAnimeDef & def,
	                              const std::vector<YootRgb> & daylight,
	                              const std::vector<YootRgb> & se,
	                              const YootSkyInputs & in,
	                              std::vector<YootRgb> & palette)
	{
		if (palette.size() < 256) palette.resize(256);
		auto put = [&](int k, const YootRgb & c) {
			const int index = def.animStartIndex + k;
			if (k >= 0 && k < def.numAnimColor && index >= 0 && index < 256)
				palette[index] = c;
		};
		// MinuteChanged refreshes the sky every 6 game minutes.
		const int minute = ((in.minuteOfDay % 1440) + 1440) % 1440;
		const int quantised = minute - minute % 6;

		if (model == YootSkyModel::Tokyo)
		{
			const std::vector<YootRgb> sky = yootTokyoSkyColors(daylight, yootTokyoSkyState(in.month, quantised, in.rainy));
			for (int k = 0; k < int(sky.size()) && k < def.numSkyColor; ++k)
				put(k, sky[k]);
		}
		else
		{
			const YootRgb filter = yootDayLightFilter(yootDayLightAngleForMinute(quantised));
			for (int k = 0; k < def.numSkyColor && k < int(daylight.size()); ++k)
				put(k, yootSkyColor(filter, daylight[k]));
			for (int k = def.numSkyColor; k < def.numSkyColor + def.numRefractColor && k < int(daylight.size()); ++k)
				put(k, yootRefractionColor(filter, daylight[k]));
		}

		// ChangeSE 0x10047a40: kind `seKind` of PALETTE 102 follows the
		// sky and refraction entries.
		if (def.numSEKind > 0 && def.numSEColor > 0)
		{
			const int kind = ((in.seKind % def.numSEKind) + def.numSEKind) % def.numSEKind;
			for (int j = 0; j < def.numSEColor; ++j)
			{
				const int src = kind * def.numSEColor + j;
				if (src < int(se.size()))
					put(def.numSkyColor + def.numRefractColor + j, se[src]);
			}
		}
	}

	// ------------------------------------------------------------------
	// Kegon seasons

	int yootKegonSeasonIndex(int month1)
	{
		if (month1 > 2 && month1 < 6) return 0;
		if (month1 > 5 && month1 < 9) return 1;
		if (month1 > 11 || month1 < 3) return 4;
		if (month1 > 8 && month1 < 11) return 2;
		return 3;
	}

	int yootKegonSkyPictId(int month1)
	{
		const int id = 200 + yootKegonSeasonIndex(month1);
		return id == 200 ? 1000 : id;
	}

	int yootKegonDayLightPaletteId(int month1)
	{
		return 150 + yootKegonSeasonIndex(month1);
	}

	int yootPaintAnimatedIndices(const std::vector<uint8_t> & indices, const std::vector<YootRgb> & palette,
	                             int start, int count, std::vector<uint8_t> & rgba)
	{
		int painted = 0;
		if (rgba.size() < indices.size() * 4) return 0;
		for (std::size_t i = 0; i < indices.size(); ++i)
		{
			const int index = indices[i];
			if (index < start || index >= start + count || index >= int(palette.size())) continue;
			const YootRgb & c = palette[index];
			rgba[i * 4 + 0] = c.r;
			rgba[i * 4 + 1] = c.g;
			rgba[i * 4 + 2] = c.b;
			rgba[i * 4 + 3] = 255;
			++painted;
		}
		return painted;
	}
}
