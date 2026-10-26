#include "Net/HttpsClient.h"

#include <httplib.h>

#include <stdexcept>

namespace ClankerCleaner::Net
{
    namespace
    {
        constexpr const char* CaCertificatesPath = "/etc/ssl/certs/ca-certificates.crt";
    }

    struct HttpsClient::PlatformState
    {
        PlatformState(const std::string& host, int port)
            : Client(host, port)
        {
        }

        httplib::SSLClient Client;
    };

    HttpsClient::HttpsClient(std::string host, int port, std::chrono::seconds timeout)
        : host(std::move(host))
        , port(port)
        , platform(std::make_unique<PlatformState>(this->host, port))
    {
        httplib::SSLClient& client = platform->Client;
        client.set_ca_cert_path(CaCertificatesPath);
        client.enable_server_certificate_verification(true);
        client.set_connection_timeout(timeout);
        client.set_read_timeout(timeout);
        client.set_write_timeout(timeout);
        client.set_keep_alive(true);
    }

    HttpsClient::~HttpsClient() = default;

    HttpResponse HttpsClient::Post(const std::string& path, const HttpHeaders& headers, const std::string& body,
                                   const std::string& contentType)
    {
        httplib::Headers requestHeaders;
        for (const auto& [name, value] : headers)
        {
            requestHeaders.emplace(name, value);
        }

        httplib::Result result = platform->Client.Post(path, requestHeaders, body, contentType);
        if (!result)
        {
            throw std::runtime_error("POST https://" + host + ":" + std::to_string(port) + path + " failed: " +
                                     httplib::to_string(result.error()));
        }
        return HttpResponse{result->status, result->body};
    }
}
