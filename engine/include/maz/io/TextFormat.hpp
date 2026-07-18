#pragma once

#include "maz/math/Math.hpp"

#include <iosfwd>
#include <string>
#include <vector>

// Shared helpers for the engine's small line-oriented text formats (.mazscene, .mazasset).
// These are deliberately minimal: whitespace-separated tokens with "double quoted" runs, and
// vec3 read/write. Keeping them in one place means every format parses names and vectors the
// same way. See docs/EDITOR.md for the .mazscene format that first used them.

namespace maz::io {

// Split a line into whitespace-separated tokens, treating a "double quoted" run as one token
// (surrounding quotes stripped, backslash escapes taken literally).
std::vector<std::string> tokenize(const std::string& line);

// Escape a string for the quoted text-file form (backslash + quote only), wrapped in quotes.
std::string quote(const std::string& s);

// Write `  <key> x y z\n` to the stream.
void writeVec3(std::ostream& os, const char* key, const math::vec3& v);

// Read a vec3 from tokens t[1..3] (t[0] is the key). Returns false if there are too few tokens.
bool readVec3(const std::vector<std::string>& t, math::vec3& out);

} // namespace maz::io
