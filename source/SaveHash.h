/* Copyright (c) 2026 OpenSkyscraper Project */
#pragma once
#include <string>

namespace OT {

/** Save-file integrity hash, ported from OpenSkyWeb (src/core/savehash.js).
 *
 * Tamper-evidence for .tower saves: an FNV-1a 32-bit hash over the exact
 * serialized XML text (excluding the hash attribute itself), salted with a
 * fixed dev salt so saves stay interoperable between the C++ game and the
 * OpenSkyWeb build (which reads the same attribute). Legacy .tower files
 * without a hash attribute still load normally.
 *
 * The hash is injected as the FIRST attribute of the root <tower> element:
 *   <tower hash="0123abcd" funds="...">...
 * and verification recomputes the hash over the text with that attribute
 * removed. Because the digest covers the file's own text, files interop
 * between both implementations as long as the salt matches. */
namespace SaveHash {

	/// Fixed development salt; matches OpenSkyWeb's DEV_SAVE_SALT default.
	extern const char * const DEV_SAVE_SALT;

	/// FNV-1a 32-bit, returned as 8 lowercase hex chars.
	std::string fnv1aHex(const std::string & s);

	/// Hash of the XML text without the hash attribute.
	std::string computeSaveHash(const std::string & xmlWithoutHash);

	/// Insert ` hash="..."` right after the root `<tower` open tag.
	/// Returns the input unchanged if no `<tower` tag is found.
	std::string injectSaveHash(const std::string & xml);

	struct VerifyResult {
		bool valid;      ///< stored hash missing (legacy) or matching.
		bool legacy;     ///< true when the file carries no hash attribute.
		std::string stored;
		std::string computed;
	};

	/// Verify the hash attribute of a raw save file text.
	VerifyResult verifySaveHash(const std::string & text);

}
}
