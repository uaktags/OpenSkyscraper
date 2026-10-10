/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "PluginObjmap.h"

#include <cstdlib>
#include <cstring>
#include <sstream>

namespace OT
{
	namespace
	{
		/// Splits `data` on `\r\n`, returning one entry per logical line. The
		/// format is CRLF-delimited, so a single `\r` (stray CR) is treated as
		/// part of its line and a bare `\n` is also accepted as a terminator
		/// for robustness against hand-edited files.
		std::vector<std::string> splitCrlfLines(const char * data, int length)
		{
			std::vector<std::string> lines;
			std::string cur;
			for (int i = 0; i < length; ++i)
			{
				char c = data[i];
				if (c == '\r')
				{
					lines.push_back(cur);
					cur.clear();
					// Swallow a following LF so we don't emit a spurious blank.
					if (i + 1 < length && data[i + 1] == '\n') ++i;
				}
				else if (c == '\n')
				{
					lines.push_back(cur);
					cur.clear();
				}
				else
				{
					cur.push_back(c);
				}
			}
			// Trailing line without a terminator.
			if (!cur.empty()) lines.push_back(cur);
			return lines;
		}

		/// Splits a line on `\t`. Empty fields are preserved (the grammar
		/// treats a leading `\t\t` as two empty fields before the key).
		std::vector<std::string> splitTabs(const std::string & line)
		{
			std::vector<std::string> out;
			std::string cur;
			for (char c : line)
			{
				if (c == '\t') { out.push_back(cur); cur.clear(); }
				else cur.push_back(c);
			}
			out.push_back(cur);
			return out;
		}

		/// Parses `Rect a,b,c,d [flags...]` from the List attribute value,
		/// filling `block.rect` and appending flag tokens to `block.flags`.
		/// Tolerates extra/missing whitespace; a malformed rect leaves the
		/// rect at zero and flags empty.
		void parseRectValue(const std::string & value, ObjmapBlock & block)
		{
			std::istringstream iss(value);
			std::string token;
			if (!(iss >> token) || token != "Rect") return;
			int v[4] = {0, 0, 0, 0};
			for (int i = 0; i < 4; ++i)
			{
				int n = 0;
				if (!(iss >> n)) return;        // truncated rect
				v[i] = n;
				char sep = 0;
				if (i < 3)
				{
					if (!(iss >> sep) || sep != ',') return;
				}
			}
			block.rect = {v[0], v[1], v[2], v[3]};
			// Remaining tokens are flags ("Transparent", "Halftone", ...).
			std::string flag;
			while (iss >> flag)
				if (!flag.empty()) block.flags.push_back(flag);
		}
	}

	bool parsePluginObjmap(const char * data, int length, std::vector<ObjmapBlock> & out)
	{
		// The format always opens with the literal "DefParts" magic. A future
		// binary variant would fail here; defensive per the doc.
		if (!data || length < 8 || std::strncmp(data, "DefParts", 8) != 0) return false;

		std::vector<ObjmapBlock> blocks;
		ObjmapBlock current;
		bool haveCurrent = false;

		const std::vector<std::string> lines = splitCrlfLines(data, length);
		for (const std::string & line : lines)
		{
			if (line == "End") break;                  // file terminator

			// A blank line closes the current block.
			if (line.empty())
			{
				if (haveCurrent) { blocks.push_back(std::move(current)); current = ObjmapBlock{}; haveCurrent = false; }
				continue;
			}

			// Block header: `DefParts\tName\t<semantic-name>`.
			if (line.rfind("DefParts\tName\t", 0) == 0)
			{
				// Close any pending block first (robust against missing blank line).
				if (haveCurrent) { blocks.push_back(std::move(current)); current = ObjmapBlock{}; }
				current.name = line.substr(std::string("DefParts\tName\t").size());
				haveCurrent = true;
				continue;
			}
			if (line == "DefParts") continue;          // degenerate header

			if (!haveCurrent) continue;                // attribute outside a block

			// Attribute line: `\t\t<key>\t<value>` (two leading tabs). After
			// splitTabs the fields are ["", "", key, value]. Some samples use
			// a single leading tab; accept either.
			const auto fields = splitTabs(line);
			if (fields.size() >= 4 && fields[0].empty() && fields[1].empty())
			{
				const std::string & key = fields[2];
				const std::string & val = fields[3];
				if      (key == "ID")      current.id      = std::atoi(val.c_str());
				else if (key == "Pattern") current.pattern = std::atoi(val.c_str());
				else if (key == "Span")    current.span    = std::atoi(val.c_str());
				else if (key == "List")    parseRectValue(val, current);
				// Unknown keys are ignored (forward-compatible).
			}
		}
		if (haveCurrent) blocks.push_back(std::move(current));

		out = std::move(blocks);
		return true;
	}

	std::string writePluginObjmap(const std::vector<ObjmapBlock> & blocks)
	{
		std::string s;
		for (const ObjmapBlock & b : blocks)
		{
			s += "DefParts\tName\t";
			s += b.name;
			s += "\r\n";
			if (b.id >= 0)      s += "\t\tID\t"      + std::to_string(b.id)      + "\r\n";
			if (b.pattern >= 0) s += "\t\tPattern\t" + std::to_string(b.pattern) + "\r\n";
			if (b.span >= 0)    s += "\t\tSpan\t"    + std::to_string(b.span)    + "\r\n";
			if (b.rect.width() != 0 || b.rect.height() != 0 || !b.flags.empty())
			{
				s += "\t\tList\tRect ";
				s += std::to_string(b.rect.left)  + ",";
				s += std::to_string(b.rect.top)   + ",";
				s += std::to_string(b.rect.right) + ",";
				s += std::to_string(b.rect.bottom);
				for (const std::string & f : b.flags) { s += " "; s += f; }
				s += "\r\n";
			}
			s += "\r\n";   // blank line ends the block
		}
		s += "End\r\n";
		return s;
	}

	ObjmapBlock * findObjmapBlock(std::vector<ObjmapBlock> & blocks,
	                              const std::string & name, int pattern)
	{
		for (ObjmapBlock & b : blocks)
		{
			if (b.name != name) continue;
			if (pattern >= 0 && b.pattern != pattern) continue;
			return &b;
		}
		return nullptr;
	}
}
