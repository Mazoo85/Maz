#pragma once

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// maz::core string utilities — the everyday text helpers Godot's String type bundles (split, join,
// strip_edges, lpad/rpad, replace, begins_with/ends_with, to_lower/upper, repeat, count). Maz already
// has StringId/StringTable for INTERNING; this is the plain manipulation layer that data parsing, UI
// text, save formats, and command handling all reach for. Pure std::string, header-only, deterministic
// — unit-tests exactly. (Locale-aware casing and Godot's quirky capitalize() are out of scope; casing
// here is ASCII.)
namespace maz::core {

// Split `s` on the substring `delim`. Empty pieces are kept unless `allowEmpty` is false. An empty
// delimiter returns the whole string as one element.
inline std::vector<std::string> split(const std::string& s, const std::string& delim,
                                      bool allowEmpty = true) {
    std::vector<std::string> out;
    if (delim.empty()) {
        out.push_back(s);
        return out;
    }
    std::size_t start = 0;
    for (;;) {
        const std::size_t hit = s.find(delim, start);
        if (hit == std::string::npos) {
            std::string piece = s.substr(start);
            if (allowEmpty || !piece.empty()) {
                out.push_back(piece);
            }
            break;
        }
        std::string piece = s.substr(start, hit - start);
        if (allowEmpty || !piece.empty()) {
            out.push_back(piece);
        }
        start = hit + delim.size();
    }
    return out;
}

// Join `parts` with `sep` between them.
inline std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) {
            out += sep;
        }
        out += parts[i];
    }
    return out;
}

inline bool beginsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}
inline bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}
inline bool contains(const std::string& s, const std::string& needle) {
    return s.find(needle) != std::string::npos;
}

// Remove `prefix` from the start of `s` only if it is actually there — Godot's String.trim_prefix.
// Returns `s` unchanged when it does not begin with `prefix` (unlike a blind substr).
inline std::string trimPrefix(const std::string& s, const std::string& prefix) {
    return beginsWith(s, prefix) ? s.substr(prefix.size()) : s;
}
// Remove `suffix` from the end of `s` only if present — Godot's String.trim_suffix. Handy for
// stripping a known extension or unit tag without touching strings that lack it.
inline std::string trimSuffix(const std::string& s, const std::string& suffix) {
    return endsWith(s, suffix) ? s.substr(0, s.size() - suffix.size()) : s;
}

// Number of pieces `s` splits into on `splitter` — Godot's String.get_slice_count. Returns 0 for an
// empty string or empty splitter; a string with no delimiter counts as 1 slice.
inline int getSliceCount(const std::string& s, const std::string& splitter) {
    if (s.empty() || splitter.empty()) {
        return 0;
    }
    int count = 1;
    std::size_t pos = 0;
    while ((pos = s.find(splitter, pos)) != std::string::npos) {
        ++count;
        pos += splitter.size();
    }
    return count;
}

// The `slice`-th piece (0-based) of `s` split on `splitter` — Godot's String.get_slice. Returns ""
// when the index is out of range, negative, or either string is empty. Empty pieces are preserved,
// so get_slice("a,,b", ",", 1) == "". Cheaper than a full split when you only need one field.
inline std::string getSlice(const std::string& s, const std::string& splitter, int slice) {
    if (s.empty() || splitter.empty() || slice < 0) {
        return std::string();
    }
    std::size_t pos = 0, prev = 0;
    int i = 0;
    while (true) {
        const std::size_t found = s.find(splitter, pos);
        const std::size_t end = (found == std::string::npos) ? s.size() : found;
        if (i == slice) {
            return s.substr(prev, end - prev);
        }
        if (found == std::string::npos) {
            break;
        }
        pos = found + splitter.size();
        prev = pos;
        ++i;
    }
    return std::string();
}

// Prepend `prefix` to every NON-EMPTY line — Godot's String.indent. Truly empty (zero-length) lines
// are left untouched; whitespace-only lines are still prefixed (matching Godot exactly).
inline std::string indent(const std::string& s, const std::string& prefix) {
    const std::vector<std::string> lines = split(s, "\n", true);
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            out += '\n';
        }
        if (!lines[i].empty()) {
            out += prefix;
        }
        out += lines[i];
    }
    return out;
}

// Strip the leading spaces/tabs from EVERY line independently — Godot's String.dedent. Note Godot
// removes ALL leading whitespace per line (not just the common minimum), so mixed indent collapses.
inline std::string dedent(const std::string& s) {
    const std::vector<std::string> lines = split(s, "\n", true);
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) {
            out += '\n';
        }
        const std::string& ln = lines[i];
        std::size_t j = 0;
        while (j < ln.size() && (ln[j] == ' ' || ln[j] == '\t')) {
            ++j;
        }
        out += ln.substr(j);
    }
    return out;
}

// Strip leading/trailing ASCII whitespace (Godot strip_edges).
inline std::string lstrip(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    return s.substr(b);
}
inline std::string rstrip(const std::string& s) {
    std::size_t e = s.size();
    while (e > 0 && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(0, e);
}
inline std::string strip(const std::string& s) { return rstrip(lstrip(s)); }

// Strip any leading characters that appear in `chars` — Godot's String.lstrip(chars). Unlike the
// whitespace-only overload above, this removes exactly the set of characters given (empty -> no-op).
inline std::string lstrip(const std::string& s, const std::string& chars) {
    std::size_t b = 0;
    while (b < s.size() && chars.find(s[b]) != std::string::npos) {
        ++b;
    }
    return s.substr(b);
}
// Strip any trailing characters that appear in `chars` — Godot's String.rstrip(chars).
inline std::string rstrip(const std::string& s, const std::string& chars) {
    std::size_t e = s.size();
    while (e > 0 && chars.find(s[e - 1]) != std::string::npos) {
        --e;
    }
    return s.substr(0, e);
}

// First `n` characters — Godot's String.left. A negative `n` counts from the end (drops the last
// -n characters); n <= 0 gives "", n >= length gives the whole string.
inline std::string left(const std::string& s, int n) {
    const int len = static_cast<int>(s.size());
    if (n < 0) {
        n = len + n;
    }
    if (n <= 0) {
        return "";
    }
    if (n >= len) {
        return s;
    }
    return s.substr(0, static_cast<std::size_t>(n));
}

// Last `n` characters — Godot's String.right. A negative `n` counts from the start (drops the first
// -n characters); n <= 0 gives "", n >= length gives the whole string.
inline std::string right(const std::string& s, int n) {
    const int len = static_cast<int>(s.size());
    if (n < 0) {
        n = len + n;
    }
    if (n <= 0) {
        return "";
    }
    if (n >= len) {
        return s;
    }
    return s.substr(static_cast<std::size_t>(len - n));
}

// Left/right pad to `width` with `fill` (Godot lpad/rpad). No-op if already at least `width` long.
inline std::string padLeft(const std::string& s, std::size_t width, char fill = ' ') {
    if (s.size() >= width) {
        return s;
    }
    return std::string(width - s.size(), fill) + s;
}
inline std::string padRight(const std::string& s, std::size_t width, char fill = ' ') {
    if (s.size() >= width) {
        return s;
    }
    return s + std::string(width - s.size(), fill);
}

// Replace every occurrence of `from` with `to` (Godot replace). Empty `from` returns `s` unchanged.
inline std::string replaceAll(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) {
        return s;
    }
    std::string out;
    std::size_t start = 0;
    for (;;) {
        const std::size_t hit = s.find(from, start);
        if (hit == std::string::npos) {
            out += s.substr(start);
            break;
        }
        out += s.substr(start, hit - start);
        out += to;
        start = hit + from.size();
    }
    return out;
}

// Strip the characters Godot forbids in a SceneTree node name — '.', ':', '@', '/', '"', '%'
// (Godot's String.validate_node_name). Each forbidden character is REMOVED (not replaced), matching
// Godot exactly; all other characters, including spaces, are preserved.
inline std::string validateNodeName(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) {
        if (c != '.' && c != ':' && c != '@' && c != '/' && c != '"' && c != '%') {
            out += c;
        }
    }
    return out;
}

inline std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}
inline std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

// Repeat `s` `n` times (Godot repeat).
inline std::string repeat(const std::string& s, std::size_t n) {
    std::string out;
    out.reserve(s.size() * n);
    for (std::size_t i = 0; i < n; ++i) {
        out += s;
    }
    return out;
}

// Count non-overlapping occurrences of `needle` in `s` (Godot count). Empty needle -> 0.
inline std::size_t count(const std::string& s, const std::string& needle) {
    if (needle.empty()) {
        return 0;
    }
    std::size_t n = 0, start = 0;
    for (;;) {
        const std::size_t hit = s.find(needle, start);
        if (hit == std::string::npos) {
            break;
        }
        ++n;
        start = hit + needle.size();
    }
    return n;
}

// Case-insensitive (ASCII) occurrence count — Godot's String.countn. Empty needle -> 0.
inline std::size_t countN(const std::string& s, const std::string& needle) {
    return count(toLower(s), toLower(needle));
}

// Case-insensitive (ASCII) search: index of the first match at/after `from`, or npos if none —
// Godot's String.findn. (Empty needle follows std::string::find, returning `from`.)
inline std::size_t findN(const std::string& s, const std::string& needle, std::size_t from = 0) {
    return toLower(s).find(toLower(needle), from);
}

// Reverse search — index of the LAST occurrence of `needle` that starts at or before `from`, or npos
// if none (Godot's String.rfind). `from == npos` (the default) searches the whole string, matching
// Godot's p_from == -1 "from the end". (Empty needle follows std::string::rfind.)
inline std::size_t rfind(const std::string& s, const std::string& needle,
                         std::size_t from = std::string::npos) {
    return s.rfind(needle, from);
}

// Case-insensitive (ASCII) reverse search — Godot's String.rfindn.
inline std::size_t rfindN(const std::string& s, const std::string& needle,
                          std::size_t from = std::string::npos) {
    return toLower(s).rfind(toLower(needle), from);
}

// ---- natural-order comparison (Godot String.naturalcasecmp_to / naturalnocasecmp_to) (M321) -----
// Numeric-aware ordering: runs of digits compare by VALUE, not character-by-character, so "file2"
// sorts before "file10" (plain lexicographic would put "file10" first because '1' < '2'). This is the
// ordering Godot uses for its FileSystem dock. Returns -1 / 0 / +1 (a < b / a == b / a > b), matching
// Godot's cmp_to sign convention. `caseSensitive == false` folds ASCII case before comparing letters.
// Note: this is the general numeric-aware comparison; it does not replicate Godot's extra leading-dot
// special case for hidden files (only relevant when one name starts with '.').
inline int naturalCompare(const std::string& a, const std::string& b, bool caseSensitive = true) {
    auto isDig = [](char c) { return c >= '0' && c <= '9'; };
    auto fold = [caseSensitive](char c) -> char {
        if (!caseSensitive && c >= 'A' && c <= 'Z') {
            return static_cast<char>(c - 'A' + 'a');
        }
        return c;
    };
    const std::size_t na = a.size(), nb = b.size();
    std::size_t i = 0, j = 0;
    while (i < na && j < nb) {
        const bool da = isDig(a[i]);
        const bool db = isDig(b[j]);
        if (da && db) {
            // Compare two digit runs numerically. Skip leading zeros first, then compare by the
            // number of significant digits, breaking ties left-to-right.
            std::size_t si = i, sj = j;
            while (si < na && a[si] == '0') ++si;
            while (sj < nb && b[sj] == '0') ++sj;
            std::size_t ei = si, ej = sj;
            while (ei < na && isDig(a[ei])) ++ei;
            while (ej < nb && isDig(b[ej])) ++ej;
            const std::size_t lenA = ei - si, lenB = ej - sj;
            if (lenA != lenB) {
                return lenA < lenB ? -1 : 1; // fewer significant digits -> smaller value
            }
            for (std::size_t k = 0; k < lenA; ++k) {
                if (a[si + k] != b[sj + k]) {
                    return a[si + k] < b[sj + k] ? -1 : 1;
                }
            }
            // Equal numeric value: the one with FEWER leading zeros sorts first (deterministic).
            const std::size_t zerosA = si - i, zerosB = sj - j;
            if (zerosA != zerosB) {
                return zerosA < zerosB ? -1 : 1;
            }
            i = ei;
            j = ej;
        } else if (da != db) {
            // A digit sorts before a non-digit at the same position (Godot's convention).
            return da ? -1 : 1;
        } else {
            const char ca = fold(a[i]), cb = fold(b[j]);
            if (ca != cb) {
                return ca < cb ? -1 : 1;
            }
            ++i;
            ++j;
        }
    }
    if (i < na) {
        return 1; // a has trailing content -> a is greater
    }
    if (j < nb) {
        return -1;
    }
    return 0;
}

// Case-insensitive natural comparison — Godot's String.naturalnocasecmp_to.
inline int naturalCompareNoCase(const std::string& a, const std::string& b) {
    return naturalCompare(a, b, false);
}

// ---- number parsing (Godot String's to_int / to_float / is_valid_* / hex_to_int) (M274) --------

// Parse a leading base-10 integer: optional leading whitespace, an optional +/- sign, then digits;
// scanning stops at the first non-digit and trailing junk is ignored (Godot's String.to_int).
// Returns 0 when no digits are present.
inline std::int64_t toInt(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    std::int64_t sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    std::int64_t v = 0;
    for (; i < s.size() && std::isdigit(static_cast<unsigned char>(s[i])); ++i) {
        v = v * 10 + (s[i] - '0');
    }
    return sign * v;
}

// True when the ENTIRE string is a base-10 integer: an optional sign followed by one or more digits,
// nothing else (Godot's String.is_valid_int).
inline bool isValidInt(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    std::size_t i = (s[0] == '+' || s[0] == '-') ? 1 : 0;
    if (i >= s.size()) {
        return false;
    }
    for (; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

// Parse a leading floating-point number (Godot's String.to_float); returns 0.0 when none is present.
inline double toFloat(const std::string& s) {
    const char* p = s.c_str();
    char* end = nullptr;
    const double v = std::strtod(p, &end);
    return end == p ? 0.0 : v;
}

// True when the ENTIRE string (bar trailing whitespace) is a valid float (Godot's is_valid_float).
inline bool isValidFloat(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    const char* p = s.c_str();
    char* end = nullptr;
    std::strtod(p, &end);
    if (end == p) {
        return false;
    }
    while (*end) {
        if (!std::isspace(static_cast<unsigned char>(*end))) {
            return false;
        }
        ++end;
    }
    return true;
}

// True when the string is a valid identifier (Godot's String.is_valid_identifier): non-empty, the
// first character a letter or '_', and every remaining character a letter, digit, or '_'. Handy for
// validating user-supplied node/variable/action names before they are used as keys.
inline bool isValidIdentifier(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const bool alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        const bool digit = c >= '0' && c <= '9';
        if (i == 0 ? !alpha : !(alpha || digit)) {
            return false;
        }
    }
    return true;
}

// True when the string is a valid HTML/hex colour (Godot's String.is_valid_html_color): an optional
// leading '#', then exactly 3, 4, 6, or 8 hexadecimal digits (RGB / RGBA / RRGGBB / RRGGBBAA).
inline bool isValidHtmlColor(const std::string& s) {
    std::size_t begin = (!s.empty() && s[0] == '#') ? 1 : 0;
    const std::size_t len = s.size() - begin;
    if (!(len == 3 || len == 4 || len == 6 || len == 8)) {
        return false;
    }
    for (std::size_t i = begin; i < s.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) {
            return false;
        }
    }
    return true;
}

// True when `needle` appears as a subsequence of `text` — its characters occur in `text` in order but
// not necessarily contiguously (Godot's String.is_subsequence_of). An empty needle matches anything.
// The NoCase variant folds ASCII case (Godot's is_subsequence_ofn).
inline bool isSubsequenceOf(const std::string& needle, const std::string& text,
                            bool caseSensitive = true) {
    if (needle.empty()) {
        return true;
    }
    auto fold = [](unsigned char c) -> unsigned char {
        return (c >= 'A' && c <= 'Z') ? static_cast<unsigned char>(c - 'A' + 'a') : c;
    };
    std::size_t n = 0;
    for (std::size_t t = 0; t < text.size() && n < needle.size(); ++t) {
        const unsigned char a = static_cast<unsigned char>(needle[n]);
        const unsigned char b = static_cast<unsigned char>(text[t]);
        if (caseSensitive ? (a == b) : (fold(a) == fold(b))) {
            ++n;
        }
    }
    return n == needle.size();
}
inline bool isSubsequenceOfNoCase(const std::string& needle, const std::string& text) {
    return isSubsequenceOf(needle, text, false);
}

// Split on `delim` and convert each piece to a float — Godot's String.split_floats. Each token is
// parsed with toFloat's leading-number rule (a non-numeric token yields 0.0). With allowEmpty=false
// empty tokens are dropped BEFORE conversion, so "1,,2" gives {1,2} rather than {1,0,2}.
inline std::vector<float> splitFloats(const std::string& s, const std::string& delim = ",",
                                      bool allowEmpty = true) {
    std::vector<float> out;
    for (const std::string& piece : split(s, delim, allowEmpty)) {
        out.push_back(static_cast<float>(toFloat(piece)));
    }
    return out;
}

// Parse a hexadecimal integer with an optional sign and optional "0x"/"0X" prefix; scanning stops at
// the first non-hex-digit (Godot's String.hex_to_int). Returns 0 when no hex digits follow.
inline std::int64_t hexToInt(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    std::int64_t sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        i += 2;
    }
    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        int d;
        if (c >= '0' && c <= '9') {
            d = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            d = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            d = c - 'A' + 10;
        } else {
            break;
        }
        v = v * 16 + d;
    }
    return sign * v;
}

// Parse a binary integer with an optional sign and optional "0b"/"0B" prefix; scanning stops at the
// first non-binary digit (Godot's String.bin_to_int). Returns 0 when no binary digits follow.
inline std::int64_t binToInt(const std::string& s) {
    std::size_t i = 0;
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
    }
    std::int64_t sign = 1;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        if (s[i] == '-') {
            sign = -1;
        }
        ++i;
    }
    if (i + 1 < s.size() && s[i] == '0' && (s[i + 1] == 'b' || s[i + 1] == 'B')) {
        i += 2;
    }
    std::int64_t v = 0;
    for (; i < s.size(); ++i) {
        const char c = s[i];
        if (c != '0' && c != '1') {
            break;
        }
        v = v * 2 + (c - '0');
    }
    return sign * v;
}

// ---- file-path helpers (Godot String's get_extension / get_basename / get_file / get_base_dir /
// path_join / simplify_path) (M285) ----------------------------------------------------------------

// The file extension without the dot (Godot's String.get_extension); empty when the final path
// component has no '.'. A leading-dot name like ".gitignore" is treated as all-extension, matching Godot.
inline std::string getExtension(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "";
    }
    const std::size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) {
        return ""; // the '.' belongs to a directory, not the file
    }
    return path.substr(dot + 1);
}

// The path with its extension removed (Godot's String.get_basename): "a/b.txt" -> "a/b".
inline std::string getBasename(const std::string& path) {
    const std::size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return path;
    }
    const std::size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos && dot < slash) {
        return path;
    }
    return path.substr(0, dot);
}

// The final path component (Godot's String.get_file): "a/b/c.txt" -> "c.txt".
inline std::string getFile(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// The directory containing the file, without a trailing slash (Godot's String.get_base_dir):
// "a/b/c.txt" -> "a/b", "/a/b" -> "/a", "c.txt" -> "".
inline std::string getBaseDir(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) {
        return "";
    }
    if (slash == 0) {
        return path.substr(0, 1); // keep the root "/"
    }
    return path.substr(0, slash);
}

// Join two path fragments with a single '/' (Godot's String.path_join).
inline std::string pathJoin(const std::string& a, const std::string& b) {
    if (a.empty()) {
        return b;
    }
    if (b.empty()) {
        return a;
    }
    const bool aSlash = a.back() == '/' || a.back() == '\\';
    const bool bSlash = b.front() == '/' || b.front() == '\\';
    if (aSlash && bSlash) {
        return a + b.substr(1);
    }
    if (aSlash || bSlash) {
        return a + b;
    }
    return a + "/" + b;
}

// Collapse redundant separators and resolve "." / ".." segments (Godot's String.simplify_path).
inline std::string simplifyPath(const std::string& path) {
    const bool absolute = !path.empty() && (path.front() == '/' || path.front() == '\\');
    std::vector<std::string> stack;
    std::string seg;
    auto flush = [&]() {
        if (seg.empty() || seg == ".") {
            seg.clear();
            return;
        }
        if (seg == "..") {
            if (!stack.empty() && stack.back() != "..") {
                stack.pop_back();
            } else if (!absolute) {
                stack.push_back("..");
            }
        } else {
            stack.push_back(seg);
        }
        seg.clear();
    };
    for (char c : path) {
        if (c == '/' || c == '\\') {
            flush();
        } else {
            seg.push_back(c);
        }
    }
    flush();
    std::string out;
    for (std::size_t i = 0; i < stack.size(); ++i) {
        if (i) {
            out.push_back('/');
        }
        out += stack[i];
    }
    if (absolute) {
        out = "/" + out;
    }
    if (out.empty()) {
        return absolute ? "/" : ".";
    }
    return out;
}

// ---- case conversion (Godot String's capitalize / to_snake_case / to_camel_case /
// to_pascal_case) (M287) ---------------------------------------------------------------------------

namespace detail {
inline bool isUpperAscii(char c) { return c >= 'A' && c <= 'Z'; }
inline bool isLowerAscii(char c) { return c >= 'a' && c <= 'z'; }
inline bool isDigitAscii(char c) { return c >= '0' && c <= '9'; }
inline bool isAlnumAscii(char c) {
    return isUpperAscii(c) || isLowerAscii(c) || isDigitAscii(c);
}

// Split into words the way Godot's case converters do: runs of non-alphanumeric characters are
// separators, and inside an alphanumeric run a boundary is inserted before an uppercase letter that
// follows a lowercase/digit (aB -> a|B) or that ends an acronym run before a lowercase (ABc -> A|Bc).
// Acronyms are thus treated as a single word ("HTTPServer" -> "HTTP","Server").
inline std::vector<std::string> splitCaseWords(const std::string& s) {
    std::vector<std::string> words;
    std::string cur;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (!isAlnumAscii(c)) {
            if (!cur.empty()) {
                words.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (!cur.empty()) {
            const char prev = cur.back();
            bool boundary = false;
            if (isUpperAscii(c) && (isLowerAscii(prev) || isDigitAscii(prev))) {
                boundary = true;
            } else if (isUpperAscii(c) && isUpperAscii(prev) && i + 1 < s.size() &&
                       isLowerAscii(s[i + 1])) {
                boundary = true;
            }
            if (boundary) {
                words.push_back(cur);
                cur.clear();
            }
        }
        cur.push_back(c);
    }
    if (!cur.empty()) {
        words.push_back(cur);
    }
    return words;
}

inline std::string firstUpperRestLower(const std::string& w) {
    std::string out = toLower(w);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}
} // namespace detail

// "move_local_x" / "camelCase" -> "Move Local X" / "Camel Case" (Godot's String.capitalize):
// splits into words, lowercases them, then capitalizes each and joins with single spaces.
inline std::string capitalize(const std::string& s) {
    const auto words = detail::splitCaseWords(s);
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i) {
            out += ' ';
        }
        out += detail::firstUpperRestLower(words[i]);
    }
    return out;
}

// "MoveLocalX" / "camelCase" -> "move_local_x" / "camel_case" (Godot's String.to_snake_case):
// lowercase words joined by underscores.
inline std::string toSnakeCase(const std::string& s) {
    const auto words = detail::splitCaseWords(s);
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i) {
        if (i) {
            out += '_';
        }
        out += toLower(words[i]);
    }
    return out;
}

// "move_local_x" -> "MoveLocalX" (Godot's String.to_pascal_case): each word capitalized, no
// separators. Acronyms are normalized ("HTTPServer" -> "HttpServer").
inline std::string toPascalCase(const std::string& s) {
    const auto words = detail::splitCaseWords(s);
    std::string out;
    for (const auto& w : words) {
        out += detail::firstUpperRestLower(w);
    }
    return out;
}

// "move_local_x" -> "moveLocalX" (Godot's String.to_camel_case): PascalCase with a lowercase first
// letter.
inline std::string toCamelCase(const std::string& s) {
    std::string out = toPascalCase(s);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[0])));
    }
    return out;
}

// ---- fuzzy matching (Godot String's similarity + a Levenshtein edit-distance utility) (M290) -----

// The consecutive 2-character substrings of `s` (Godot's String.bigrams): "night" -> ni,ig,gh,ht.
// Empty for strings shorter than 2 characters.
inline std::vector<std::string> bigrams(const std::string& s) {
    std::vector<std::string> out;
    if (s.size() < 2) {
        return out;
    }
    out.reserve(s.size() - 1);
    for (std::size_t i = 0; i + 1 < s.size(); ++i) {
        out.push_back(s.substr(i, 2));
    }
    return out;
}

// Sørensen–Dice bigram similarity in [0,1] — Godot's String.similarity. 1.0 for identical strings,
// 0.0 when either string is shorter than 2 characters; otherwise 2*|shared bigrams| / (total bigrams),
// matching each target bigram at most once (Godot's exact algorithm).
inline double similarity(const std::string& a, const std::string& b) {
    if (a == b) {
        return 1.0;
    }
    if (a.size() < 2 || b.size() < 2) {
        return 0.0;
    }
    std::vector<std::string> src = bigrams(a);
    std::vector<std::string> tgt = bigrams(b);
    const double sum = static_cast<double>(src.size() + tgt.size());
    double inter = 0.0;
    for (const auto& sb : src) {
        for (auto& tb : tgt) {
            if (!tb.empty() && sb == tb) {
                inter += 1.0;
                tb.clear(); // consume this target bigram so it can't match twice
                break;
            }
        }
    }
    return (2.0 * inter) / sum;
}

// Levenshtein edit distance: the minimum number of single-character insertions, deletions or
// substitutions to turn `a` into `b`. A general fuzzy-matching utility (beyond Godot's String API,
// which offers only `similarity`). O(len(a)*len(b)) time, O(len(b)) space.
inline std::size_t levenshtein(const std::string& a, const std::string& b) {
    const std::size_t n = a.size();
    const std::size_t m = b.size();
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }
    std::vector<std::size_t> prev(m + 1), cur(m + 1);
    for (std::size_t j = 0; j <= m; ++j) {
        prev[j] = j;
    }
    for (std::size_t i = 1; i <= n; ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= m; ++j) {
            const std::size_t cost = (a[i - 1] == b[j - 1]) ? 0u : 1u;
            const std::size_t del = prev[j] + 1;
            const std::size_t ins = cur[j - 1] + 1;
            const std::size_t sub = prev[j - 1] + cost;
            std::size_t best = del < ins ? del : ins;
            if (sub < best) {
                best = sub;
            }
            cur[j] = best;
        }
        std::swap(prev, cur);
    }
    return prev[m];
}

// ---- markup / URI escaping (Godot String's xml_escape / xml_unescape / uri_encode / uri_decode)
// (M289) -------------------------------------------------------------------------------------------

// Escape the XML/HTML metacharacters & < > (and, when escapeQuotes, " ') — Godot's String.xml_escape.
inline std::string xmlEscape(const std::string& s, bool escapeQuotes = false) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;"; break;
        case '>': out += "&gt;"; break;
        case '"': out += escapeQuotes ? "&quot;" : "\""; break;
        case '\'': out += escapeQuotes ? "&apos;" : "'"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

// Reverse xmlEscape: resolve the named entities &amp;/&lt;/&gt;/&quot;/&apos; and numeric character
// references &#DDD; and &#xHHH; (Godot's String.xml_unescape). ASCII code points only; an
// unrecognized or malformed '&...' sequence is left verbatim.
inline std::string xmlUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '&') {
            out.push_back(s[i++]);
            continue;
        }
        const std::size_t semi = s.find(';', i);
        if (semi == std::string::npos) {
            out.push_back(s[i++]);
            continue;
        }
        const std::string ent = s.substr(i + 1, semi - i - 1);
        bool handled = true;
        if (ent == "amp") {
            out.push_back('&');
        } else if (ent == "lt") {
            out.push_back('<');
        } else if (ent == "gt") {
            out.push_back('>');
        } else if (ent == "quot") {
            out.push_back('"');
        } else if (ent == "apos") {
            out.push_back('\'');
        } else if (ent.size() >= 2 && ent[0] == '#') {
            long code = -1;
            if (ent[1] == 'x' || ent[1] == 'X') {
                code = std::strtol(ent.c_str() + 2, nullptr, 16);
            } else {
                code = std::strtol(ent.c_str() + 1, nullptr, 10);
            }
            if (code >= 0 && code <= 0x10FFFF) {
                out.push_back(static_cast<char>(code & 0xFF));
            } else {
                handled = false;
            }
        } else {
            handled = false;
        }
        if (handled) {
            i = semi + 1;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

// Percent-encode every byte except the RFC 3986 unreserved set (A-Za-z0-9 and - _ . ~) — Godot's
// String.uri_encode. Bytes are emitted as uppercase %XX.
inline std::string uriEncode(const std::string& s) {
    static const char* hexd = "0123456789ABCDEF";
    std::string out;
    out.reserve(s.size() * 3);
    for (char ch : s) {
        const unsigned char c = static_cast<unsigned char>(ch);
        const bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
                                c == '~';
        if (unreserved) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hexd[(c >> 4) & 0xF]);
            out.push_back(hexd[c & 0xF]);
        }
    }
    return out;
}

// Decode %XX escapes back to bytes — Godot's String.uri_decode. A '+' is left as-is (Godot does not
// treat it as a space), and a malformed '%' with fewer than two hex digits after it is kept verbatim.
inline std::string uriDecode(const std::string& s) {
    auto hv = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '%' && i + 2 < s.size()) {
            const int a = hv(s[i + 1]);
            const int b = hv(s[i + 2]);
            if (a >= 0 && b >= 0) {
                out.push_back(static_cast<char>(a * 16 + b));
                i += 3;
                continue;
            }
        }
        out.push_back(s[i++]);
    }
    return out;
}

// ---- Number formatting (M305) — Godot String.num / pad_decimals / pad_zeros / humanize_size ----
//
// Godot's String carries a small set of number-to-text helpers that data display, save formats, and
// debug HUDs lean on constantly. These reproduce that behaviour exactly (including Godot's quirks:
// pad_decimals/pad_zeros are pure string surgery that TRUNCATE rather than round, and humanize_size
// uses a strict `>` so an exact 1024-multiple stays in the smaller unit). Deterministic, unit-tested.

// Format a value with exactly `decimals` fractional digits (rounded) — Godot's String.num(value, d).
// `decimals` <= 0 yields the rounded integer with no decimal point.
inline std::string numToString(double value, int decimals = 0) {
    if (decimals < 0) {
        decimals = 0;
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, value);
    return std::string(buf);
}

// Godot's String.pad_decimals: make the fractional part exactly `digits` long by TRUNCATING extra
// digits (no rounding) or padding with trailing zeros. `digits` <= 0 drops the fractional part and
// the decimal point. Operates on the string as-is (feed it a plain decimal numeral).
inline std::string padDecimals(const std::string& s, int digits) {
    std::string out = s;
    std::size_t dot = out.find('.');
    if (digits <= 0) {
        return dot == std::string::npos ? out : out.substr(0, dot);
    }
    if (dot == std::string::npos) {
        out += '.';
        dot = out.size() - 1;
    }
    const std::size_t have = out.size() - (dot + 1);
    const std::size_t want = static_cast<std::size_t>(digits);
    if (have > want) {
        out = out.substr(0, dot + 1 + want); // truncate — matches Godot (no rounding)
    } else {
        out.append(want - have, '0');
    }
    return out;
}

// Godot's String.pad_zeros: left-pad the INTEGER part with zeros so it is at least `digits` long,
// inserting after any leading sign and leaving the fractional part untouched.
inline std::string padZeros(const std::string& s, int digits) {
    std::string out = s;
    std::size_t end = out.find('.');
    if (end == std::string::npos) {
        end = out.size();
    }
    std::size_t begin = 0;
    while (begin < end && !(out[begin] >= '0' && out[begin] <= '9')) {
        ++begin; // skip a leading sign / non-digit
    }
    if (begin >= end) {
        return out; // no integer digits to pad
    }
    const std::size_t intDigits = end - begin;
    if (static_cast<std::size_t>(digits) > intDigits) {
        out.insert(begin, static_cast<std::size_t>(digits) - intDigits, '0');
    }
    return out;
}

// Godot's String.humanize_size: byte count -> human-readable binary units (B, KiB, MiB, ... EiB),
// two decimals above bytes, using Godot's strict `>` step so an exact 1024-multiple stays in the
// smaller unit (e.g. 1024 -> "1024 B", 1048576 -> "1024.00 KiB").
inline std::string humanizeSize(std::uint64_t bytes) {
    static const char* const prefixes[] = {" B", " KiB", " MiB", " GiB", " TiB", " PiB", " EiB"};
    std::uint64_t div = 1;
    int idx = 0;
    while (bytes > div * 1024 && idx < 6) {
        div *= 1024;
        ++idx;
    }
    const int digits = idx > 0 ? 2 : 0;
    const double value = static_cast<double>(bytes) / static_cast<double>(div);
    return numToString(value, digits) + prefixes[idx];
}

// ---- Wildcard glob matching (M317) — Godot String.match / matchn ------------------------------
//
// Case-sensitive (match) and case-insensitive (matchn) shell-style wildcard matching, replicating
// Godot's exact _wildcard_match semantics: `*` matches any run (including empty), `?` matches any
// single character EXCEPT '.', every other character matches literally. Two Godot quirks are kept
// faithfully: (1) an empty pattern OR an empty subject always returns false, and (2) `?` deliberately
// won't match a dot (handy for extension-aware globs). Used for file filters and name patterns.
namespace detail {
inline char lowerAscii(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
inline bool wildcardMatch(const char* pat, const char* str, bool caseSensitive) {
    switch (*pat) {
    case '\0':
        return *str == '\0';
    case '*':
        return wildcardMatch(pat + 1, str, caseSensitive) ||
               (*str != '\0' && wildcardMatch(pat, str + 1, caseSensitive));
    case '?':
        return *str != '\0' && *str != '.' && wildcardMatch(pat + 1, str + 1, caseSensitive);
    default:
        return (caseSensitive ? (*str == *pat) : (lowerAscii(*str) == lowerAscii(*pat))) &&
               wildcardMatch(pat + 1, str + 1, caseSensitive);
    }
}
} // namespace detail

// Godot's String.match (case-sensitive) / matchn (matchGlob with caseSensitive=false). Empty
// pattern or empty subject -> false (Godot's guard).
inline bool matchGlob(const std::string& s, const std::string& pattern, bool caseSensitive = true) {
    if (pattern.empty() || s.empty()) {
        return false;
    }
    return detail::wildcardMatch(pattern.c_str(), s.c_str(), caseSensitive);
}

// ---- C-string escaping (M308) — Godot String.c_escape / c_unescape ----------------------------
//
// The escaping Godot uses when it writes a string into a text resource (.tscn/.tres) or any
// C-style literal: control characters and quotes become backslash sequences and back. c_escape maps
// the backslash itself plus the bell/backspace/formfeed/newline/carriage-return/tab/vtab controls and
// both quote marks; c_unescape reverses that (also accepting \? -> ?, which some emitters produce).
// The pair round-trips exactly. Deterministic, header-only, unit-tested.

// Escape control characters and quotes into C-style backslash sequences — Godot's String.c_escape.
inline std::string cEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char ch : s) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '\a': out += "\\a"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\v': out += "\\v"; break;
        case '\'': out += "\\'"; break;
        case '"': out += "\\\""; break;
        default: out.push_back(ch); break;
        }
    }
    return out;
}

// Reverse cEscape: turn C-style backslash sequences back into their characters — Godot's c_unescape.
// A backslash before an unrecognized character keeps that character verbatim (drops the backslash),
// and a trailing lone backslash is kept as-is.
inline std::string cUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    std::size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            const char n = s[i + 1];
            switch (n) {
            case '\\': out.push_back('\\'); break;
            case 'a': out.push_back('\a'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'v': out.push_back('\v'); break;
            case '\'': out.push_back('\''); break;
            case '"': out.push_back('"'); break;
            case '?': out.push_back('?'); break;
            default: out.push_back(n); break; // unknown escape: keep the char, drop the backslash
            }
            i += 2;
        } else {
            out.push_back(s[i++]);
        }
    }
    return out;
}

} // namespace maz::core
