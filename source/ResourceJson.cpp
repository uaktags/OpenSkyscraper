/* Copyright (c) 2026 OpenSkyscraper contributors */
#include "ResourceJson.h"

#include <cstdio>

namespace OT
{
	namespace
	{
		const Json kNull;

		void appendUtf8(std::string & out, uint32_t cp)
		{
			if (cp < 0x80) out.push_back(char(cp));
			else if (cp < 0x800)
			{
				out.push_back(char(0xC0 | (cp >> 6)));
				out.push_back(char(0x80 | (cp & 0x3F)));
			}
			else if (cp < 0x10000)
			{
				out.push_back(char(0xE0 | (cp >> 12)));
				out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
				out.push_back(char(0x80 | (cp & 0x3F)));
			}
			else
			{
				out.push_back(char(0xF0 | (cp >> 18)));
				out.push_back(char(0x80 | ((cp >> 12) & 0x3F)));
				out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
				out.push_back(char(0x80 | (cp & 0x3F)));
			}
		}

		void escapeString(std::string & out, const std::string & s)
		{
			out.push_back('"');
			for (unsigned char c : s)
			{
				switch (c)
				{
					case '"':  out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\n': out += "\\n"; break;
					case '\r': out += "\\r"; break;
					case '\t': out += "\\t"; break;
					default:
						if (c < 0x20)
						{
							char buf[8];
							std::snprintf(buf, sizeof(buf), "\\u%04x", c);
							out += buf;
						}
						else out.push_back(char(c));
				}
			}
			out.push_back('"');
		}

		class Parser
		{
		public:
			explicit Parser(const std::string & s) : s_(s) {}

			bool document(Json & out)
			{
				ws();
				if (!value(out, 0)) return false;
				ws();
				if (p_ != s_.size()) return fail("trailing characters");
				return true;
			}

			std::string error;

		private:
			const std::string & s_;
			size_t p_ = 0;

			bool fail(const char * what)
			{
				if (error.empty()) error = std::string(what) + " at offset " + std::to_string(p_);
				return false;
			}

			void ws()
			{
				while (p_ < s_.size() && (s_[p_] == ' ' || s_[p_] == '\t' || s_[p_] == '\n' || s_[p_] == '\r')) ++p_;
			}

			bool literal(const char * lit)
			{
				size_t n = 0;
				while (lit[n]) ++n;
				if (s_.compare(p_, n, lit) != 0) return false;
				p_ += n;
				return true;
			}

			bool hex4(uint32_t & v)
			{
				if (p_ + 4 > s_.size()) return fail("short \\u escape");
				v = 0;
				for (int i = 0; i < 4; ++i)
				{
					const char c = s_[p_++];
					v <<= 4;
					if (c >= '0' && c <= '9') v |= uint32_t(c - '0');
					else if (c >= 'a' && c <= 'f') v |= uint32_t(c - 'a' + 10);
					else if (c >= 'A' && c <= 'F') v |= uint32_t(c - 'A' + 10);
					else return fail("bad \\u escape");
				}
				return true;
			}

			bool string(std::string & out)
			{
				if (p_ >= s_.size() || s_[p_] != '"') return fail("expected string");
				++p_;
				while (p_ < s_.size())
				{
					const char c = s_[p_++];
					if (c == '"') return true;
					if (c != '\\') { out.push_back(c); continue; }
					if (p_ >= s_.size()) break;
					const char e = s_[p_++];
					switch (e)
					{
						case '"': out.push_back('"'); break;
						case '\\': out.push_back('\\'); break;
						case '/': out.push_back('/'); break;
						case 'b': out.push_back('\b'); break;
						case 'f': out.push_back('\f'); break;
						case 'n': out.push_back('\n'); break;
						case 'r': out.push_back('\r'); break;
						case 't': out.push_back('\t'); break;
						case 'u':
						{
							uint32_t cp = 0;
							if (!hex4(cp)) return false;
							if (cp >= 0xD800 && cp < 0xDC00 && p_ + 6 <= s_.size() && s_[p_] == '\\' && s_[p_ + 1] == 'u')
							{
								const size_t save = p_;
								p_ += 2;
								uint32_t lo = 0;
								if (hex4(lo) && lo >= 0xDC00 && lo < 0xE000)
									cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
								else
								{
									p_ = save;
									error.clear();
								}
							}
							appendUtf8(out, cp);
							break;
						}
						default: return fail("bad escape");
					}
				}
				return fail("unterminated string");
			}

			bool value(Json & out, int depth)
			{
				if (depth > 200) return fail("nesting too deep");
				ws();
				if (p_ >= s_.size()) return fail("unexpected end");
				const char c = s_[p_];
				if (c == '{')
				{
					++p_;
					out = Json::object();
					ws();
					if (p_ < s_.size() && s_[p_] == '}') { ++p_; return true; }
					for (;;)
					{
						ws();
						std::string key;
						if (!string(key)) return false;
						ws();
						if (p_ >= s_.size() || s_[p_] != ':') return fail("expected ':'");
						++p_;
						Json v;
						if (!value(v, depth + 1)) return false;
						out.members().emplace_back(std::move(key), std::move(v));
						ws();
						if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
						if (p_ < s_.size() && s_[p_] == '}') { ++p_; return true; }
						return fail("expected ',' or '}'");
					}
				}
				if (c == '[')
				{
					++p_;
					out = Json::array();
					ws();
					if (p_ < s_.size() && s_[p_] == ']') { ++p_; return true; }
					for (;;)
					{
						Json v;
						if (!value(v, depth + 1)) return false;
						out.elements().push_back(std::move(v));
						ws();
						if (p_ < s_.size() && s_[p_] == ',') { ++p_; continue; }
						if (p_ < s_.size() && s_[p_] == ']') { ++p_; return true; }
						return fail("expected ',' or ']'");
					}
				}
				if (c == '"')
				{
					std::string str;
					if (!string(str)) return false;
					out = Json(std::move(str));
					return true;
				}
				if (literal("true")) { out = Json(true); return true; }
				if (literal("false")) { out = Json(false); return true; }
				if (literal("null")) { out = Json(); return true; }
				if (c == '-' || (c >= '0' && c <= '9'))
				{
					bool neg = false;
					if (c == '-') { neg = true; ++p_; }
					if (p_ >= s_.size() || s_[p_] < '0' || s_[p_] > '9') return fail("bad number");
					unsigned long long v = 0;
					while (p_ < s_.size() && s_[p_] >= '0' && s_[p_] <= '9')
					{
						const unsigned d = unsigned(s_[p_] - '0');
						if (v > (~0ull - d) / 10) return fail("number too large");
						v = v * 10 + d;
						++p_;
					}
					if (p_ < s_.size() && (s_[p_] == '.' || s_[p_] == 'e' || s_[p_] == 'E'))
						return fail("fractional numbers are not supported");
					if (neg ? v > 0x8000000000000000ull : v > 0x7FFFFFFFFFFFFFFFull) return fail("number out of range");
					out = Json(neg ? (long long)(0 - v) : (long long)v);
					return true;
				}
				return fail("unexpected character");
			}
		};
	}

	const Json & Json::at(size_t i) const
	{
		return (type_ == Type::Array && i < arr_.size()) ? arr_[i] : kNull;
	}

	const Json * Json::find(const std::string & key) const
	{
		if (type_ != Type::Object) return nullptr;
		for (const Member & m : obj_)
			if (m.first == key) return &m.second;
		return nullptr;
	}

	Json * Json::find(const std::string & key)
	{
		if (type_ != Type::Object) return nullptr;
		for (Member & m : obj_)
			if (m.first == key) return &m.second;
		return nullptr;
	}

	const Json & Json::operator[](const std::string & key) const
	{
		const Json * j = find(key);
		return j ? *j : kNull;
	}

	Json & Json::set(const std::string & key, Json v)
	{
		if (type_ != Type::Object) *this = object();
		if (Json * j = find(key)) *j = std::move(v);
		else obj_.emplace_back(key, std::move(v));
		return *this;
	}

	bool Json::operator==(const Json & o) const
	{
		if (type_ != o.type_) return false;
		switch (type_)
		{
			case Type::Null:   return true;
			case Type::Bool:
			case Type::Int:    return int_ == o.int_;
			case Type::String: return str_ == o.str_;
			case Type::Array:  return arr_ == o.arr_;
			case Type::Object: return obj_ == o.obj_;
		}
		return false;
	}

	void Json::dumpTo(std::string & out, int indent, int depth) const
	{
		auto newline = [&](int d) {
			if (indent < 0) return;
			out.push_back('\n');
			out.append(size_t(indent * d), ' ');
		};
		switch (type_)
		{
			case Type::Null:   out += "null"; return;
			case Type::Bool:   out += int_ ? "true" : "false"; return;
			case Type::Int:    out += std::to_string(int_); return;
			case Type::String: escapeString(out, str_); return;
			case Type::Array:
			{
				if (arr_.empty()) { out += "[]"; return; }
				bool scalars = true;
				for (const Json & e : arr_)
					if (e.isArray() || e.isObject()) { scalars = false; break; }
				out.push_back('[');
				for (size_t i = 0; i < arr_.size(); ++i)
				{
					if (i) out += scalars || indent < 0 ? ", " : ",";
					if (!scalars) newline(depth + 1);
					arr_[i].dumpTo(out, indent, depth + 1);
				}
				if (!scalars) newline(depth);
				out.push_back(']');
				return;
			}
			case Type::Object:
			{
				if (obj_.empty()) { out += "{}"; return; }
				out.push_back('{');
				for (size_t i = 0; i < obj_.size(); ++i)
				{
					if (i) out.push_back(',');
					if (indent < 0 && i) out.push_back(' ');
					newline(depth + 1);
					escapeString(out, obj_[i].first);
					out += ": ";
					obj_[i].second.dumpTo(out, indent, depth + 1);
				}
				newline(depth);
				out.push_back('}');
				return;
			}
		}
	}

	std::string Json::dump(int indent) const
	{
		std::string out;
		dumpTo(out, indent, 0);
		if (indent >= 0) out.push_back('\n');
		return out;
	}

	bool Json::parse(const std::string & text, Json & out, std::string * error)
	{
		Parser p(text);
		Json tmp;
		if (!p.document(tmp))
		{
			if (error) *error = p.error;
			return false;
		}
		out = std::move(tmp);
		return true;
	}

	std::string latin1ToUtf8(const std::string & bytes)
	{
		std::string out;
		out.reserve(bytes.size());
		for (unsigned char c : bytes) appendUtf8(out, c);
		return out;
	}

	bool utf8ToLatin1(const std::string & utf8, std::string & bytes)
	{
		std::string out;
		out.reserve(utf8.size());
		for (size_t i = 0; i < utf8.size(); ++i)
		{
			const unsigned char c = uint8_t(utf8[i]);
			if (c < 0x80) { out.push_back(char(c)); continue; }
			if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size() && (uint8_t(utf8[i + 1]) & 0xC0) == 0x80)
			{
				const uint32_t cp = (uint32_t(c & 0x1F) << 6) | (uint8_t(utf8[i + 1]) & 0x3F);
				if (cp > 0xFF) return false;
				out.push_back(char(cp));
				++i;
				continue;
			}
			return false;
		}
		bytes = std::move(out);
		return true;
	}

	std::string toHex(const std::string & bytes)
	{
		static const char * const kDigits = "0123456789abcdef";
		std::string out;
		out.reserve(bytes.size() * 2);
		for (unsigned char c : bytes)
		{
			out.push_back(kDigits[c >> 4]);
			out.push_back(kDigits[c & 15]);
		}
		return out;
	}

	bool fromHex(const std::string & hex, std::string & bytes)
	{
		if (hex.size() % 2) return false;
		std::string out;
		out.reserve(hex.size() / 2);
		auto nib = [](char c) -> int {
			if (c >= '0' && c <= '9') return c - '0';
			if (c >= 'a' && c <= 'f') return c - 'a' + 10;
			if (c >= 'A' && c <= 'F') return c - 'A' + 10;
			return -1;
		};
		for (size_t i = 0; i < hex.size(); i += 2)
		{
			const int hi = nib(hex[i]), lo = nib(hex[i + 1]);
			if (hi < 0 || lo < 0) return false;
			out.push_back(char((hi << 4) | lo));
		}
		bytes = std::move(out);
		return true;
	}
}
