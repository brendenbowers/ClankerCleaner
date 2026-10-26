#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// A keep-alive HTTPS connection to one host. HttpsClient.RPi.cpp implements it with
// cpp-httplib and OpenSSL, verifying the server against the system's CA certificates.
namespace ClankerCleaner::Net
{
    using HttpHeaders = std::vector<std::pair<std::string, std::string>>;

    struct HttpResponse
    {
        int Status = 0;
        std::string Body;
    };

    class HttpsClient
    {
    public:
        HttpsClient(std::string host, int port, std::chrono::seconds timeout);
        ~HttpsClient();

        HttpsClient(const HttpsClient&) = delete;
        HttpsClient& operator=(const HttpsClient&) = delete;

        const std::string& GetHost() const { return host; }
        int GetPort() const { return port; }

        // Not thread-safe; use one client per thread. Throws std::runtime_error when the
        // request can't be sent or no response arrives (DNS, TLS, timeout). A gzip body
        // is decompressed before it's returned.
        HttpResponse Post(const std::string& path, const HttpHeaders& headers, const std::string& body,
                          const std::string& contentType);

    private:
        struct PlatformState;

        std::string host;
        int port;
        std::unique_ptr<PlatformState> platform;
    };
}
