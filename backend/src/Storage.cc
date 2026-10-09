#include "Storage.h"

#include <drogon/drogon.h>

#include <algorithm>
#include <filesystem>

#include "Config.h"

namespace fs = std::filesystem;

namespace files {
namespace {
FileRow rowToFile(const drogon::orm::Row& r) {
    FileRow f;
    f.id = r["id"].as<long long>();
    f.key = r["key"].as<std::string>();
    f.name = r["name"].as<std::string>();
    f.size = static_cast<unsigned long long>(r["size"].as<long long>());
    f.createdAt = r["created_at"].as<std::string>();
    return f;
}
const char* kColumns = "id, key, name, size, created_at";
}  // namespace

std::string filesDir() { return config().dataDir + "/files"; }
std::string pathFor(const std::string& key) { return filesDir() + "/" + key; }

std::string newKey() {
    std::string u = drogon::utils::getUuid();
    u.erase(std::remove(u.begin(), u.end(), '-'), u.end());
    return u;
}

void prepareStorage() {
    std::error_code ec;
    fs::create_directories(filesDir(), ec);
    // Interrupted uploads: half-written files and the temporary bodies Drogon spools big requests into.
    for (const auto& e : fs::directory_iterator(filesDir(), ec))
        if (e.path().extension() == ".part") fs::remove(e.path(), ec);
    const auto tmp = fs::path(config().dataDir) / "tmp";
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);

    auto db = drogon::app().getDbClient();
    db->execSqlSync("PRAGMA journal_mode=WAL;");
    db->execSqlSync("PRAGMA busy_timeout=5000;");
    db->execSqlSync(
        "CREATE TABLE IF NOT EXISTS files ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT, "
        "key TEXT NOT NULL UNIQUE, "
        "name TEXT NOT NULL, "
        "size INTEGER NOT NULL, "
        "created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')))");
}

unsigned long long totalBytes() {
    auto r = drogon::app().getDbClient()->execSqlSync("SELECT COALESCE(SUM(size), 0) AS total FROM files");
    return static_cast<unsigned long long>(r[0]["total"].as<long long>());
}

unsigned long long fileCount() {
    auto r = drogon::app().getDbClient()->execSqlSync("SELECT COUNT(*) AS n FROM files");
    return static_cast<unsigned long long>(r[0]["n"].as<long long>());
}

std::optional<FileRow> findFile(long long id) {
    auto r = drogon::app().getDbClient()->execSqlSync(
        std::string("SELECT ") + kColumns + " FROM files WHERE id = ?", id);
    if (r.empty()) return std::nullopt;
    return rowToFile(r[0]);
}

std::vector<FileRow> listFiles(long long before, long long limit) {
    auto r = drogon::app().getDbClient()->execSqlSync(
        std::string("SELECT ") + kColumns + " FROM files WHERE id < ? ORDER BY id DESC LIMIT ?", before, limit);
    std::vector<FileRow> out;
    for (const auto& row : r) out.push_back(rowToFile(row));
    return out;
}

long long insertFile(const std::string& key, const std::string& name, unsigned long long size) {
    auto r = drogon::app().getDbClient()->execSqlSync(
        "INSERT INTO files (key, name, size) VALUES (?, ?, ?)", key, name, static_cast<long long>(size));
    return static_cast<long long>(r.insertId());
}

void deleteRow(long long id) { drogon::app().getDbClient()->execSqlSync("DELETE FROM files WHERE id = ?", id); }

Json::Value toJson(const FileRow& f) {
    Json::Value o;
    o["id"] = static_cast<Json::Int64>(f.id);
    o["name"] = f.name;
    o["size"] = static_cast<Json::UInt64>(f.size);
    o["createdAt"] = f.createdAt;
    return o;
}
}  // namespace files
