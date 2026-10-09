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
}  // namespace files
