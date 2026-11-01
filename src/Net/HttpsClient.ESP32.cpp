// Placeholder until the cloud client is ported (Config.ESP32.cpp keeps [dreame] off): every
// request throws. The port would use esp_http_client with the certificate bundle.

#include "Net/HttpsClient.h"

#include <stdexcept>

namespace ClankerCleaner::Net
{
    struct HttpsClient::PlatformState
    {
    };

    HttpsClient::HttpsClient(std::string host, int port, std::chrono::seconds /*timeout*/)
        : host(std::move(host))
        , port(port)
        , platform(std::make_unique<PlatformState>())
    {
    }

    HttpsClient::~HttpsClient() = default;

    HttpResponse HttpsClient::Post(const std::string& path, const HttpHeaders& /*headers*/, const std::string& /*body*/,
                                   const std::string& /*contentType*/)
    {
        throw std::runtime_error("POST https://" + host + ":" + std::to_string(port) + path + " failed: HTTPS isn't ported to the ESP32 yet");
    }
}
