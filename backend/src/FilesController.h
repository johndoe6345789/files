#pragma once
#include <drogon/HttpController.h>

namespace files {
class FilesController : public drogon::HttpController<FilesController> {
public:
    using Callback = std::function<void(const drogon::HttpResponsePtr&)>;

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(FilesController::health, "/api/health", drogon::Get);
    ADD_METHOD_TO(FilesController::info, "/api/info", drogon::Get);
    ADD_METHOD_TO(FilesController::list, "/api/files", drogon::Get);
    ADD_METHOD_TO(FilesController::upload, "/api/files", drogon::Post);
    ADD_METHOD_TO(FilesController::download, "/api/files/{1}/download", drogon::Get);
    // Not exposed by the portal: for the operator, via `docker exec` into the backend container.
    ADD_METHOD_TO(FilesController::remove, "/api/files/{1}", drogon::Delete);
    METHOD_LIST_END

    void health(const drogon::HttpRequestPtr& req, Callback&& cb);
    void info(const drogon::HttpRequestPtr& req, Callback&& cb);
    void list(const drogon::HttpRequestPtr& req, Callback&& cb);
    void upload(const drogon::HttpRequestPtr& req, Callback&& cb);
    void download(const drogon::HttpRequestPtr& req, Callback&& cb, std::string&& id);
    void remove(const drogon::HttpRequestPtr& req, Callback&& cb, std::string&& id);
};
}  // namespace files
