#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>

namespace files {
// Limits. The per-file limit must match "client_max_body_size" in config.json and the portal's nginx.
constexpr std::uint64_t kMaxFileBytes = 10ull * 1024 * 1024 * 1024;
constexpr std::size_t kMaxNameBytes = 200;

inline std::string envOr(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return v && *v ? v : fallback;
}

struct Config {
    std::string dataDir;           // FILES_DIR, default /data (config.json's db and upload_path live there too)
    std::uint64_t maxTotalBytes;   // FILES_MAX_TOTAL_BYTES, default 100 GiB
};

inline const Config& config() {
    static const Config c{envOr("FILES_DIR", "/data"),
                          std::strtoull(envOr("FILES_MAX_TOTAL_BYTES", "107374182400").c_str(), nullptr, 10)};
    return c;
}
}  // namespace files
