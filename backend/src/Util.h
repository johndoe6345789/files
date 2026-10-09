#pragma once
#include <string>
#include <string_view>

namespace files {
// A safe display name for an uploaded file: no directory part, no control or bidi-override characters
// (they can disguise an extension), valid UTF-8, at most kMaxNameBytes bytes, never empty.
std::string sanitizeName(std::string_view raw);

// Content-Disposition value forcing a download: an ASCII fallback plus an RFC 5987 UTF-8 name.
std::string contentDisposition(const std::string& name);

// Parses a whole decimal string as an unsigned number; false on empty, junk, or overflow.
bool parseU64(std::string_view s, unsigned long long& out);

// A parsed `Range: bytes=...` header for a file of `size` bytes.
struct ByteRange {
    enum class Kind { None, Satisfiable, Unsatisfiable } kind = Kind::None;
    unsigned long long start = 0, length = 0;
};
// Single ranges only: "a-b", "a-" and "-n". Anything else (several ranges, other units, junk) is ignored, which makes
// the caller send the whole file, as the HTTP spec allows. A well-formed range outside the file is Unsatisfiable (416).
ByteRange parseRange(std::string_view header, unsigned long long size);
}  // namespace files
