#include "Util.h"

#include <charconv>
#include <cstdint>

#include "Config.h"

namespace files {
namespace {
constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;

// Decodes one code point at s[i], advancing i. Invalid or overlong input yields kInvalid and advances one byte.
std::uint32_t nextCodePoint(std::string_view s, std::size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) { ++i; return c; }
    int extra;
    std::uint32_t cp, min;
    if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; min = 0x10000; }
    else { ++i; return kInvalid; }
    if (i + extra >= s.size()) { ++i; return kInvalid; }
    for (int k = 1; k <= extra; ++k) {
        const unsigned char d = static_cast<unsigned char>(s[i + k]);
        if ((d & 0xC0) != 0x80) { ++i; return kInvalid; }
        cp = (cp << 6) | (d & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) { ++i; return kInvalid; }
    i += extra + 1;
    return cp;
}

std::size_t utf8Len(std::uint32_t cp) { return cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4; }

void appendCodePoint(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) out += static_cast<char>(cp);
    else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
    else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18)); out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// Controls, C1 controls, zero-width and bidi formatting characters, line/paragraph separators, BOM, noncharacters.
bool dropped(std::uint32_t cp) {
    return cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F) || (cp >= 0x200B && cp <= 0x200F) ||
           cp == 0x2028 || cp == 0x2029 || (cp >= 0x202A && cp <= 0x202E) || (cp >= 0x2060 && cp <= 0x2064) ||
           (cp >= 0x2066 && cp <= 0x2069) || cp == 0xFEFF || (cp >= 0xFFF9 && cp <= 0xFFFB) || cp == 0xFFFE ||
           cp == 0xFFFF;
}
}  // namespace

std::string sanitizeName(std::string_view raw) {
    const auto slash = raw.find_last_of("/\\");
    if (slash != std::string_view::npos) raw = raw.substr(slash + 1);
    std::string out;
    std::size_t i = 0;
    while (i < raw.size()) {
        std::uint32_t cp = nextCodePoint(raw, i);
        if (cp == kInvalid) cp = '_';
        if (dropped(cp)) continue;
        if (out.size() + utf8Len(cp) > kMaxNameBytes) break;
        appendCodePoint(out, cp);
    }
    const auto first = out.find_first_not_of(' ');
    if (first == std::string::npos) return "file";
    out = out.substr(first, out.find_last_not_of(' ') - first + 1);
    if (out.find_first_not_of('.') == std::string::npos) return "file";
    return out;
}

std::string contentDisposition(const std::string& name) {
    static const char* hex = "0123456789ABCDEF";
    std::string ascii, pct;
    for (std::size_t i = 0; i < name.size();) {
        const std::size_t start = i;
        const std::uint32_t cp = nextCodePoint(name, i);
        if (cp != kInvalid && cp >= 0x20 && cp < 0x7F && cp != '"' && cp != '\\' && cp != '%' && cp != ';')
            ascii += static_cast<char>(cp);
        else
            ascii += '_';
        for (std::size_t k = start; k < i; ++k) {
            const unsigned char b = static_cast<unsigned char>(name[k]);
            const bool keep = (b >= 'A' && b <= 'Z') || (b >= 'a' && b <= 'z') || (b >= '0' && b <= '9') ||
                              std::string_view("!#$&+-.^_`|~").find(static_cast<char>(b)) != std::string_view::npos;
            if (keep) pct += static_cast<char>(b);
            else { pct += '%'; pct += hex[b >> 4]; pct += hex[b & 15]; }
        }
    }
    return "attachment; filename=\"" + ascii + "\"; filename*=UTF-8''" + pct;
}

bool parseU64(std::string_view s, unsigned long long& out) {
    if (s.empty()) return false;
    unsigned long long v = 0;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
    if (r.ec != std::errc() || r.ptr != s.data() + s.size()) return false;
    out = v;
    return true;
}
}  // namespace files
