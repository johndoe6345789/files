#include "Upload.h"

#include <drogon/drogon.h>

#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <cstdio>
#include <filesystem>

#include "Config.h"
#include "Storage.h"
#include "Util.h"

using namespace drogon;
namespace fs = std::filesystem;

namespace files {
namespace {
using Callback = std::function<void(const HttpResponsePtr&)>;
constexpr unsigned long long kFlushEvery = 16ull * 1024 * 1024;

HttpResponsePtr errorResponse(HttpStatusCode code, const std::string& message) {
    Json::Value b;
    b["error"] = message;
    auto r = HttpResponse::newHttpJsonResponse(b);
    r->setStatusCode(code);
    r->addHeader("Cache-Control", "no-store");
    return r;
}

// One upload in flight. Whatever happens, the half-written file is removed when this goes away.
struct Upload {
    Callback cb;
    std::string name, key, path, part;
    std::FILE* file = nullptr;
    unsigned long long written = 0, baseTotal = 0, flushedUpTo = 0;
    HttpStatusCode failCode = k200OK;
    std::string failMsg;

    bool failed() const { return failCode != k200OK; }
    void discard() {
        if (file) { std::fclose(file); file = nullptr; }
        if (!part.empty()) { std::error_code ec; fs::remove(part, ec); part.clear(); }
    }
    // Remembers the first problem and frees the disk space straight away; the answer goes out when the body is done.
    void fail(HttpStatusCode code, const std::string& msg) {
        if (!failed()) { failCode = code; failMsg = msg; }
        discard();
    }
    ~Upload() { discard(); }
};

void finish(const std::shared_ptr<Upload>& up, std::exception_ptr ep) {
    if (ep) {  // the client went away or sent something Drogon could not parse
        up->discard();
        return up->cb(errorResponse(k400BadRequest, "the upload was interrupted"));
    }
    if (up->failed()) {
        const auto code = up->failCode;
        const auto msg = up->failMsg;
        up->discard();
        return up->cb(errorResponse(code, msg));
    }
    if (up->written == 0) {
        up->discard();
        return up->cb(errorResponse(k400BadRequest, "the file is empty"));
    }
    const bool flushed = std::fflush(up->file) == 0;
    const bool closed = std::fclose(up->file) == 0;
    up->file = nullptr;
    std::error_code ec;
    if (!flushed || !closed || (fs::rename(up->part, up->path, ec), ec)) {
        LOG_ERROR << "finishing " << up->part << " failed";
        up->discard();
        return up->cb(errorResponse(k500InternalServerError, "could not store the file"));
    }
    up->part.clear();  // it is the real file now
    try {
        const long long id = insertFile(up->key, up->name, up->written);
        auto row = findFile(id);
        LOG_INFO << "stored file " << id << " (" << up->written << " bytes)";
        auto r = HttpResponse::newHttpJsonResponse(toJson(*row));
        r->setStatusCode(k201Created);
        r->addHeader("Cache-Control", "no-store");
        up->cb(r);
    } catch (const std::exception& e) {
        fs::remove(up->path, ec);
        LOG_ERROR << "database insert failed: " << e.what();
        up->cb(errorResponse(k500InternalServerError, "could not store the file"));
    }
}
}  // namespace

void uploadStream(const HttpRequestPtr& req, RequestStreamPtr&& stream, Callback&& callback) {
    auto up = std::make_shared<Upload>();
    up->cb = std::move(callback);

    const std::string& rawName = req->getParameter("name");
    if (rawName.empty()) up->fail(k400BadRequest, "missing ?name=");
    up->name = sanitizeName(rawName);

    try {
        up->baseTotal = totalBytes();
        // The announced size lets us refuse early; the count while writing is what is actually enforced.
        unsigned long long announced = 0;
        if (!up->failed() && parseU64(req->getHeader("content-length"), announced)) {
            if (announced > kMaxFileBytes) up->fail(k413RequestEntityTooLarge, "the file is too large");
            else if (up->baseTotal + announced > config().maxTotalBytes) up->fail(k507InsufficientStorage, "the storage is full");
        }
        if (!up->failed()) {
            up->key = newKey();
            up->path = pathFor(up->key);
            up->part = up->path + ".part";
            up->file = std::fopen(up->part.c_str(), "wb");
            if (!up->file) {
                LOG_ERROR << "cannot create " << up->part << ": " << std::strerror(errno);
                up->part.clear();
                up->fail(k500InternalServerError, "could not store the file");
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "upload setup failed: " << e.what();
        up->fail(k500InternalServerError, "could not store the file");
    }

    // A failed upload still reads (and drops) the rest of the body, so the answer reaches a client that is still sending.
    stream->setStreamReader(RequestStreamReader::newReader(
        [up](const char* data, size_t n) {
            if (up->failed()) return;
            up->written += n;
            if (up->written > kMaxFileBytes) return up->fail(k413RequestEntityTooLarge, "the file is too large");
            if (up->baseTotal + up->written > config().maxTotalBytes) return up->fail(k507InsufficientStorage, "the storage is full");
            int err = std::fwrite(data, 1, n, up->file) == n ? 0 : errno;
            // Keep the kernel's page cache from growing with the file. Left alone, a 10 GB upload fills the container's
            // memory limit with dirty pages whenever the disk is slower than the network, and the container is killed.
            // So every 16 MiB: write it out, then tell the kernel it may forget those pages. This also slows the
            // sender to the speed of the disk instead of buffering the difference.
            if (!err && up->written - up->flushedUpTo >= kFlushEvery) {
                const int fd = fileno(up->file);
                if (std::fflush(up->file) != 0 || fdatasync(fd) != 0) err = errno ? errno : EIO;
                else {
                    posix_fadvise(fd, static_cast<off_t>(up->flushedUpTo), static_cast<off_t>(up->written - up->flushedUpTo), POSIX_FADV_DONTNEED);
                    up->flushedUpTo = up->written;
                }
            }
            if (err) {
                LOG_ERROR << "write failed: " << std::strerror(err);
                up->fail(err == ENOSPC ? k507InsufficientStorage : k500InternalServerError,
                         err == ENOSPC ? "the storage is full" : "could not store the file");
            }
        },
        [up](std::exception_ptr ep) { finish(up, ep); }));
}
}  // namespace files
