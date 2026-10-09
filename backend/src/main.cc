#include <drogon/drogon.h>

#include "Storage.h"

int main() {
    drogon::app().loadConfigFile("/app/config.json");
    drogon::app().registerBeginningAdvice([]() { files::prepareStorage(); });
    drogon::app().run();
    return 0;
}
