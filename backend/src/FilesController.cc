#include "FilesController.h"

#include <drogon/drogon.h>

#include <climits>
#include <filesystem>
#include <fstream>

#include "Config.h"
#include "Storage.h"
#include "Util.h"

using namespace drogon;
namespace fs = std::filesystem;

namespace files {
namespace {
HttpResponsePtr json(const Json::Value& body, HttpStatusCode code = k200OK) {
    auto r = HttpResponse::newHttpJsonResponse(body);
    r->setStatusCode(code);
    r->addHeader("Cache-Control", "no-store");
    return r;
}
HttpResponsePtr error(HttpStatusCode code, const std::string& message) {
    Json::Value b;
    b["error"] = message;
    return json(b, code);
}
bool parseId(const std::string& s, long long& id) {
    unsigned long long v;
    if (!parseU64(s, v) || v > static_cast<unsigned long long>(LLONG_MAX)) return false;
    id = static_cast<long long>(v);
    return true;
}
}  // namespace

void FilesController::health(const HttpRequestPtr&, Callback&& cb) {
    Json::Value b;
    b["status"] = "ok";
    cb(json(b));
}

void FilesController::info(const HttpRequestPtr&, Callback&& cb) {
    Json::Value b;
    b["maxFileBytes"] = static_cast<Json::UInt64>(kMaxFileBytes);
    b["maxTotalBytes"] = static_cast<Json::UInt64>(config().maxTotalBytes);
    b["files"] = static_cast<Json::UInt64>(fileCount());
    b["bytes"] = static_cast<Json::UInt64>(totalBytes());
    cb(json(b));
}

// GET /api/files?limit=50&before=<id>: newest first. `next` is the cursor for the following page, or null.
void FilesController::list(const HttpRequestPtr& req, Callback&& cb) {
    unsigned long long limit = 50, before = static_cast<unsigned long long>(LLONG_MAX);
    const auto& l = req->getParameter("limit");
    const auto& b = req->getParameter("before");
    if (!l.empty() && !parseU64(l, limit)) return cb(error(k400BadRequest, "limit must be a number"));
    if (!b.empty() && (!parseU64(b, before) || before > static_cast<unsigned long long>(LLONG_MAX)))
        return cb(error(k400BadRequest, "before must be a file id"));
    limit = std::min<unsigned long long>(std::max<unsigned long long>(limit, 1), 100);

    auto rows = listFiles(static_cast<long long>(before), static_cast<long long>(limit) + 1);
    Json::Value out;
    out["items"] = Json::Value(Json::arrayValue);
    out["next"] = Json::Value();
    for (std::size_t i = 0; i < rows.size() && i < limit; ++i) out["items"].append(toJson(rows[i]));
    if (rows.size() > limit) out["next"] = static_cast<Json::Int64>(rows[limit - 1].id);
    cb(json(out));
}

// GET /api/files/<id>/download: always an attachment of opaque bytes, so a file can never run as a page on this origin.
void FilesController::download(const HttpRequestPtr& req, Callback&& cb, std::string&& id) {
    long long n;
    if (!parseId(id, n)) return cb(error(k404NotFound, "no such file"));
    auto row = findFile(n);
    std::error_code ec;
    if (!row) return cb(error(k404NotFound, "no such file"));
    const std::string path = pathFor(row->key);
    const auto size = static_cast<unsigned long long>(fs::file_size(path, ec));
    if (ec) return cb(error(k404NotFound, "no such file"));

    // Resumable downloads: a 10 GB file that stops at 9 GB must not start over. The file never changes once stored.
    const auto range = parseRange(req->getHeader("range"), size);
    if (range.kind == ByteRange::Kind::Unsatisfiable) {
        auto r = error(k416RequestedRangeNotSatisfiable, "that range is outside the file");
        r->addHeader("Content-Range", "bytes */" + std::to_string(size));
        return cb(r);
    }
    HttpResponsePtr resp;
    if (range.kind == ByteRange::Kind::Satisfiable) {
        resp = HttpResponse::newFileResponse(path, range.start, range.length, false, "", CT_APPLICATION_OCTET_STREAM, "", req);
        resp->setStatusCode(k206PartialContent);
        resp->addHeader("Content-Range", "bytes " + std::to_string(range.start) + "-" +
                                             std::to_string(range.start + range.length - 1) + "/" + std::to_string(size));
    } else {
        resp = HttpResponse::newFileResponse(path, "", CT_APPLICATION_OCTET_STREAM, "", req);
    }
    resp->addHeader("Accept-Ranges", "bytes");
    resp->addHeader("Content-Disposition", contentDisposition(row->name));
    resp->addHeader("X-Content-Type-Options", "nosniff");
    resp->addHeader("Content-Security-Policy", "sandbox; default-src 'none'");
    resp->addHeader("Cache-Control", "public, max-age=3600");
    cb(resp);
}

// DELETE /api/files/<id>
void FilesController::remove(const HttpRequestPtr&, Callback&& cb, std::string&& id) {
    long long n;
    if (!parseId(id, n)) return cb(error(k404NotFound, "no such file"));
    auto row = findFile(n);
    if (!row) return cb(error(k404NotFound, "no such file"));
    std::error_code ec;
    fs::remove(pathFor(row->key), ec);
    deleteRow(n);
    LOG_INFO << "deleted file " << n;
    Json::Value b;
    b["deleted"] = static_cast<Json::Int64>(n);
    cb(json(b));
}
}  // namespace files
