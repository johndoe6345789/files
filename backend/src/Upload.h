#pragma once
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/RequestStream.h>

#include <functional>

namespace files {
// POST /api/files?name=<file name>: the body is the file. A streaming handler: Drogon hands over the body in pieces as
// it arrives and each piece goes straight into the file's final place, so a 10 GB upload needs no temporary copy, no
// memory proportional to the size and no waiting for a copy at the end. Registered in main.cc (not a controller method:
// Drogon only supports streaming handlers as plain functions).
void uploadStream(const drogon::HttpRequestPtr& req, drogon::RequestStreamPtr&& stream,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);
}  // namespace files
