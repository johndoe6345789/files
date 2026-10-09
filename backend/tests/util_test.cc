// Plain asserts, no framework: the Docker build runs this and fails on the first wrong result.
#include <cstdlib>
#include <iostream>

#include "Util.h"

using namespace files;
static int failures = 0;
#define CHECK_EQ(actual, expected)                                                                         \
    do {                                                                                                   \
        const auto a = (actual);                                                                           \
        const std::string e = (expected);                                                                  \
        if (a != e) { ++failures; std::cerr << "FAIL line " << __LINE__ << ": got [" << a << "] want [" << e << "]\n"; } \
    } while (0)

int main() {
    // directory parts and traversal are dropped
    CHECK_EQ(sanitizeName("../../etc/passwd"), "passwd");
    CHECK_EQ(sanitizeName("C:\\Users\\me\\photo.jpg"), "photo.jpg");
    CHECK_EQ(sanitizeName("a/b/"), "file");
    CHECK_EQ(sanitizeName(".."), "file");
    CHECK_EQ(sanitizeName("..."), "file");
    CHECK_EQ(sanitizeName(".bashrc"), ".bashrc");
    CHECK_EQ(sanitizeName(""), "file");
    CHECK_EQ(sanitizeName("   "), "file");
    CHECK_EQ(sanitizeName("  padded name.txt  "), "padded name.txt");
    // control characters and newlines
    CHECK_EQ(sanitizeName(std::string("a\nb\r\tc\0d", 8)), "abcd");
    CHECK_EQ(sanitizeName("bell\x07.txt"), "bell.txt");
    // right-to-left override, the classic "exe disguised as txt" trick, and zero-width characters
    CHECK_EQ(sanitizeName("report\xE2\x80\xAEtxt.exe"), "reporttxt.exe");
    CHECK_EQ(sanitizeName("a\xE2\x80\x8B" "b"), "ab");
    CHECK_EQ(sanitizeName("\xEF\xBB\xBF" "bom.txt"), "bom.txt");
    // unicode is kept
    CHECK_EQ(sanitizeName("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x93\x84.txt"), "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x93\x84.txt");
    // invalid UTF-8 becomes '_', and overlong or surrogate encodings are rejected
    CHECK_EQ(sanitizeName("a\xFF" "b"), "a_b");
    CHECK_EQ(sanitizeName("a\xC0\xAF" "b"), "a__b");
    CHECK_EQ(sanitizeName("a\xED\xA0\x80" "b"), "a___b");
    CHECK_EQ(sanitizeName("trunc\xE2\x82"), "trunc__");
    // length limit counts bytes and never splits a character
    {
        std::string longName(500, 'x');
        if (sanitizeName(longName).size() != 200) { ++failures; std::cerr << "FAIL long ascii\n"; }
        std::string euros;
        for (int i = 0; i < 100; ++i) euros += "\xE2\x82\xAC";  // 3 bytes each
        const auto r = sanitizeName(euros);
        if (r.size() != 198 || r.size() % 3 != 0) { ++failures; std::cerr << "FAIL multibyte cut: " << r.size() << "\n"; }
    }
    // Content-Disposition
    CHECK_EQ(contentDisposition("plain.txt"), "attachment; filename=\"plain.txt\"; filename*=UTF-8''plain.txt");
    CHECK_EQ(contentDisposition("a b\"c;d%e.txt"),
             "attachment; filename=\"a b_c_d_e.txt\"; filename*=UTF-8''a%20b%22c%3Bd%25e.txt");
    CHECK_EQ(contentDisposition("caf\xC3\xA9.txt"), "attachment; filename=\"caf_.txt\"; filename*=UTF-8''caf%C3%A9.txt");
    CHECK_EQ(contentDisposition("back\\slash"), "attachment; filename=\"back_slash\"; filename*=UTF-8''back%5Cslash");
    // numbers
    unsigned long long v = 0;
    if (!parseU64("0", v) || v != 0) { ++failures; std::cerr << "FAIL parse 0\n"; }
    if (!parseU64("18446744073709551615", v) || v != 18446744073709551615ull) { ++failures; std::cerr << "FAIL parse max\n"; }
    if (parseU64("18446744073709551616", v)) { ++failures; std::cerr << "FAIL parse overflow\n"; }
    for (const char* bad : {"", "-1", "1x", " 1", "0x10", "1.5"})
        if (parseU64(bad, v)) { ++failures; std::cerr << "FAIL parse accepted [" << bad << "]\n"; }

    // Range header
    {
        auto expect = [&](const char* hdr, unsigned long long size, ByteRange::Kind kind, unsigned long long start, unsigned long long len, int line) {
            const auto r = parseRange(hdr, size);
            if (r.kind != kind || (kind == ByteRange::Kind::Satisfiable && (r.start != start || r.length != len))) {
                ++failures; std::cerr << "FAIL range line " << line << " [" << hdr << "] got kind " << int(r.kind) << " " << r.start << "+" << r.length << "\n";
            }
        };
        using K = ByteRange::Kind;
        expect("bytes=0-9", 100, K::Satisfiable, 0, 10, __LINE__);
        expect("bytes=10-", 100, K::Satisfiable, 10, 90, __LINE__);
        expect("bytes=-5", 100, K::Satisfiable, 95, 5, __LINE__);
        expect("bytes=-500", 100, K::Satisfiable, 0, 100, __LINE__);       // longer than the file: all of it
        expect("bytes=90-999", 100, K::Satisfiable, 90, 10, __LINE__);     // end clipped to the file
        expect("bytes=0-0", 100, K::Satisfiable, 0, 1, __LINE__);
        expect("bytes=99-99", 100, K::Satisfiable, 99, 1, __LINE__);
        expect("bytes=10737418240-10737418250", 10737418300ull, K::Satisfiable, 10737418240ull, 11, __LINE__);  // beyond 4 GiB
        expect("bytes=100-", 100, K::Unsatisfiable, 0, 0, __LINE__);
        expect("bytes=100-200", 100, K::Unsatisfiable, 0, 0, __LINE__);
        expect("bytes=-0", 100, K::Unsatisfiable, 0, 0, __LINE__);
        expect("bytes=0-5", 0, K::Unsatisfiable, 0, 0, __LINE__);          // nothing to range over
        expect("bytes=5-2", 100, K::None, 0, 0, __LINE__);                 // backwards: ignored
        expect("bytes=0-1,5-6", 100, K::None, 0, 0, __LINE__);             // several ranges: ignored
        expect("items=0-1", 100, K::None, 0, 0, __LINE__);
        expect("bytes=abc", 100, K::None, 0, 0, __LINE__);
        expect("bytes=-", 100, K::None, 0, 0, __LINE__);
        expect("bytes=", 100, K::None, 0, 0, __LINE__);
        expect("", 100, K::None, 0, 0, __LINE__);
        expect("  bytes=1-2  ", 100, K::Satisfiable, 1, 2, __LINE__);
    }

    if (failures) { std::cerr << failures << " check(s) failed\n"; return 1; }
    std::cout << "util_test: all checks passed\n";
    return 0;
}
