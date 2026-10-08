/* Copyright (c) 2026 OpenSkyscraper contributors */
///
/// A small ordered JSON document model for the plugin data layer.
///
/// Used by the OSP v3 writer/reader (OspPlugin.h) to store every decoded
/// YootTower resource as human-readable JSON, by the CResFile schema engine
/// for the world resources (WorldResourceCodec.h) and by the Plugin Editor,
/// which edits resources through this tree.
///
/// Scope is deliberately narrow: numbers are 64-bit integers only (no
/// plugin field is fractional), object keys keep their insertion order so a
/// written file reads in the engine's field order, and strings are UTF-8
/// byte strings. Plugin text is Windows code-page bytes, not UTF-8; the
/// resource layer maps those bytes through Latin-1 (`latin1ToUtf8`) so any
/// byte sequence survives a JSON round trip.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace OT
{
	class Json
	{
	public:
		enum class Type { Null, Bool, Int, String, Array, Object };
		using Member = std::pair<std::string, Json>;

		Json() = default;
		Json(std::nullptr_t) {}
		Json(bool b) : type_(Type::Bool), int_(b ? 1 : 0) {}
		Json(int v) : type_(Type::Int), int_(v) {}
		Json(unsigned v) : type_(Type::Int), int_(v) {}
		Json(long v) : type_(Type::Int), int_(v) {}
		Json(unsigned long v) : type_(Type::Int), int_(int64_t(v)) {}
		Json(long long v) : type_(Type::Int), int_(v) {}
		Json(unsigned long long v) : type_(Type::Int), int_(int64_t(v)) {}
		Json(const char * s) : type_(Type::String), str_(s ? s : "") {}
		Json(std::string s) : type_(Type::String), str_(std::move(s)) {}

		static Json array() { Json j; j.type_ = Type::Array; return j; }
		static Json object() { Json j; j.type_ = Type::Object; return j; }

		Type type() const { return type_; }
		bool isNull() const { return type_ == Type::Null; }
		bool isBool() const { return type_ == Type::Bool; }
		bool isInt() const { return type_ == Type::Int; }
		bool isString() const { return type_ == Type::String; }
		bool isArray() const { return type_ == Type::Array; }
		bool isObject() const { return type_ == Type::Object; }

		int64_t asInt(int64_t def = 0) const { return (type_ == Type::Int || type_ == Type::Bool) ? int_ : def; }
		bool asBool(bool def = false) const { return (type_ == Type::Int || type_ == Type::Bool) ? int_ != 0 : def; }
		const std::string & asString() const { return str_; }

		// --- arrays ---
		size_t size() const { return type_ == Type::Array ? arr_.size() : type_ == Type::Object ? obj_.size() : 0; }
		const std::vector<Json> & elements() const { return arr_; }
		std::vector<Json> & elements() { return arr_; }
		void push(Json v) { if (type_ != Type::Array) { *this = array(); } arr_.push_back(std::move(v)); }
		const Json & at(size_t i) const;

		// --- objects (insertion ordered) ---
		const std::vector<Member> & members() const { return obj_; }
		std::vector<Member> & members() { return obj_; }
		/// Member by key, or nullptr.
		const Json * find(const std::string & key) const;
		Json * find(const std::string & key);
		/// Member by key; a static Null when absent.
		const Json & operator[](const std::string & key) const;
		/// Sets (or appends) a member and returns *this for chaining.
		Json & set(const std::string & key, Json v);
		bool has(const std::string & key) const { return find(key) != nullptr; }

		bool operator==(const Json & o) const;
		bool operator!=(const Json & o) const { return !(*this == o); }

		/// Serialises the tree. `indent` < 0 gives one line. Arrays whose
		/// elements are all scalars are written on one line either way.
		std::string dump(int indent = 1) const;
		/// Parses a document. On failure returns false and sets `error`
		/// (with the byte offset).
		static bool parse(const std::string & text, Json & out, std::string * error = nullptr);

	private:
		Type type_ = Type::Null;
		int64_t int_ = 0;
		std::string str_;
		std::vector<Json> arr_;
		std::vector<Member> obj_;

		void dumpTo(std::string & out, int indent, int depth) const;
	};

	/// Plugin byte string -> UTF-8 via Latin-1 (every byte becomes U+0000..U+00FF).
	std::string latin1ToUtf8(const std::string & bytes);
	/// Inverse of latin1ToUtf8. Fails on a code point above U+00FF or bad UTF-8.
	bool utf8ToLatin1(const std::string & utf8, std::string & bytes);

	/// Lower-case hex of a byte string ("" for empty) and its inverse.
	std::string toHex(const std::string & bytes);
	bool fromHex(const std::string & hex, std::string & bytes);
}
