#include <drogon/drogon.h>

#include "Storage.h"
#include "Upload.h"

int main() {
    drogon::app().loadConfigFile("/app/config.json");
    // Streaming lets the upload handler receive a body of any size in pieces. Other handlers are unaffected: Drogon
    // collects the body for them as before.
    drogon::app().enableRequestStream();
    drogon::app().registerHandler("/api/files", &files::uploadStream, {drogon::Post});
    drogon::app().registerBeginningAdvice([]() { files::prepareStorage(); });
    drogon::app().run();
    return 0;
}
