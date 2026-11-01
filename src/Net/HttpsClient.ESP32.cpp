// ESP32 HttpsClient, on esp_http_client with ESP-IDF's bundle of common root certificates.
// esp_http_client doesn't decompress, so a gzip body is inflated here with the ROM's miniz.

#include "Net/HttpsClient.h"

#include <array>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <miniz.h>

namespace ClankerCleaner::Net
{
    namespace
    {
        constexpr std::size_t ReadChunkSize = 1024;

        // Gzip framing (RFC 1952) around a raw deflate stream.
        constexpr std::size_t GzipHeaderSize = 10;
        constexpr std::size_t GzipTrailerSize = 8;
        constexpr std::uint8_t GzipHasHeaderCrc = 0x02;
        constexpr std::uint8_t GzipHasExtra = 0x04;
        constexpr std::uint8_t GzipHasName = 0x08;
        constexpr std::uint8_t GzipHasComment = 0x10;

        bool IsGzip(const std::string& body)
        {
            return body.size() >= 2 && static_cast<std::uint8_t>(body[0]) == 0x1f && static_cast<std::uint8_t>(body[1]) == 0x8b;
        }

        std::string Gunzip(const std::string& compressed)
        {
            const auto byteAt = [&compressed](std::size_t index) { return static_cast<std::uint8_t>(compressed[index]); };
            if(compressed.size() < GzipHeaderSize + GzipTrailerSize || byteAt(2) != 8)
            {
                throw std::runtime_error("the gzip body isn't valid");
            }

            const std::uint8_t flags = byteAt(3);
            const std::size_t end = compressed.size() - GzipTrailerSize;
            std::size_t position = GzipHeaderSize;
            if(flags & GzipHasExtra)
            {
                position += 2 + (position + 1 < end ? (byteAt(position) | (byteAt(position + 1) << 8)) : 0);
            }
            for(const std::uint8_t field : {GzipHasName, GzipHasComment})
            {
                if(flags & field)
                {
                    position = compressed.find('\0', position);
                    position = position == std::string::npos ? end + 1 : position + 1;
                }
            }
            if(flags & GzipHasHeaderCrc)
            {
                position += 2;
            }
            if(position > end)
            {
                throw std::runtime_error("the gzip body isn't valid");
            }

            // The trailer ends with the uncompressed size, so the output is allocated once.
            const std::uint32_t size = byteAt(end + 4) | (byteAt(end + 5) << 8) | (byteAt(end + 6) << 16) |
                                       (static_cast<std::uint32_t>(byteAt(end + 7)) << 24);
            std::string inflated(size, '\0');

            // About 11 KB, too much for a thread's stack.
            auto decompressor = std::make_unique<tinfl_decompressor>();
            tinfl_init(decompressor.get());
            std::size_t inputSize = end - position;
            std::size_t outputSize = inflated.size();
            auto* output = reinterpret_cast<mz_uint8*>(inflated.data());
            const tinfl_status status = tinfl_decompress(decompressor.get(), reinterpret_cast<const mz_uint8*>(compressed.data()) + position,
                                                         &inputSize, output, output, &outputSize, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
            if(status != TINFL_STATUS_DONE || outputSize != size)
            {
                throw std::runtime_error("the gzip body didn't decompress");
            }
            return inflated;
        }
    }

    struct HttpsClient::PlatformState
    {
        ~PlatformState()
        {
            if(Client)
            {
                esp_http_client_cleanup(Client);
            }
        }

        esp_http_client_handle_t Client = nullptr;
        // Headers stay set on the client, so the last request's are removed before the next.
        std::vector<std::string> HeaderNames;
    };

    HttpsClient::HttpsClient(std::string host, int port, std::chrono::seconds timeout)
        : host(std::move(host))
        , port(port)
        , platform(std::make_unique<PlatformState>())
    {
        esp_http_client_config_t config{};
        config.host = this->host.c_str();
        config.port = port;
        config.path = "/";
        config.transport_type = HTTP_TRANSPORT_OVER_SSL;
        config.crt_bundle_attach = esp_crt_bundle_attach;
        config.timeout_ms = static_cast<int>(std::chrono::milliseconds(timeout).count());
        config.method = HTTP_METHOD_POST;
        // Like cpp-httplib's default: a redirect comes back as the response.
        config.disable_auto_redirect = true;
        config.keep_alive_enable = true;

        platform->Client = esp_http_client_init(&config);
        if(!platform->Client)
        {
            throw std::runtime_error("HTTPS client for " + this->host + " couldn't be created");
        }
    }

    HttpsClient::~HttpsClient() = default;

    HttpResponse HttpsClient::Post(const std::string& path, const HttpHeaders& headers, const std::string& body,
                                   const std::string& contentType)
    {
        esp_http_client_handle_t client = platform->Client;
        const std::string url = "https://" + host + ":" + std::to_string(port) + path;
        const auto fail = [&](const std::string& reason)
        {
            // Drops the connection, so the next request starts on a fresh one.
            esp_http_client_close(client);
            return std::runtime_error("POST " + url + " failed: " + reason);
        };

        for(const std::string& name : platform->HeaderNames)
        {
            esp_http_client_delete_header(client, name.c_str());
        }
        platform->HeaderNames.clear();
        const auto setHeader = [&](const std::string& name, const std::string& value)
        {
            esp_http_client_set_header(client, name.c_str(), value.c_str());
            platform->HeaderNames.push_back(name);
        };
        for(const auto& [name, value] : headers)
        {
            setHeader(name, value);
        }
        setHeader("Content-Type", contentType);

        if(esp_http_client_set_url(client, url.c_str()) != ESP_OK)
        {
            throw fail("the URL isn't valid");
        }
        esp_http_client_set_method(client, HTTP_METHOD_POST);

        // A kept-alive connection the server has since closed fails before any response
        // arrives; that's retried once, on a new connection.
        for(int attempt = 1;; ++attempt)
        {
            const bool lastAttempt = attempt == 2;
            const esp_err_t openResult = esp_http_client_open(client, static_cast<int>(body.size()));
            if(openResult != ESP_OK)
            {
                const std::runtime_error error = fail(esp_err_to_name(openResult));
                if(lastAttempt)
                {
                    throw error;
                }
                continue;
            }

            std::size_t written = 0;
            while(written < body.size())
            {
                const int result = esp_http_client_write(client, body.data() + written, static_cast<int>(body.size() - written));
                if(result <= 0)
                {
                    break;
                }
                written += static_cast<std::size_t>(result);
            }
            if(written < body.size() || esp_http_client_fetch_headers(client) < 0)
            {
                const std::runtime_error error = fail(written < body.size() ? "the request couldn't be sent" : "no response");
                if(lastAttempt)
                {
                    throw error;
                }
                continue;
            }
            break;
        }

        HttpResponse response;
        response.Status = esp_http_client_get_status_code(client);
        std::array<char, ReadChunkSize> chunk;
        int result = 0;
        while((result = esp_http_client_read(client, chunk.data(), static_cast<int>(chunk.size()))) > 0)
        {
            response.Body.append(chunk.data(), static_cast<std::size_t>(result));
        }
        if(result < 0 || !esp_http_client_is_complete_data_received(client))
        {
            throw fail("the response was cut off");
        }

        if(IsGzip(response.Body))
        {
            try
            {
                response.Body = Gunzip(response.Body);
            }
            catch(const std::runtime_error& error)
            {
                throw fail(error.what());
            }
        }
        return response;
    }
}
