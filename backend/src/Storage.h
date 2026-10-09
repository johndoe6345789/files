#pragma once
#include <json/json.h>

#include <optional>
#include <string>
#include <vector>

namespace files {
struct FileRow {
    long long id = 0;
    std::string key;  // random name of the file on disk
    std::string name; // sanitised display name
    unsigned long long size = 0;
    std::string createdAt;  // ISO 8601, UTC
};

// Creates the data directories, removes leftovers of interrupted uploads, creates the table.
void prepareStorage();

std::string filesDir();
std::string pathFor(const std::string& key);
std::string newKey();

unsigned long long totalBytes();
unsigned long long fileCount();
std::optional<FileRow> findFile(long long id);
// Newest first: rows with id < before, at most `limit`.
std::vector<FileRow> listFiles(long long before, long long limit);
long long insertFile(const std::string& key, const std::string& name, unsigned long long size);
void deleteRow(long long id);

Json::Value toJson(const FileRow& f);
}  // namespace files
