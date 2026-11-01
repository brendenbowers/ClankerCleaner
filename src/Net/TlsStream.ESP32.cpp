// ESP32 TlsStream, on esp-tls (mbedTLS). The server is checked against ESP-IDF's bundle of
// common root certificates, or against the PEM given to Connect.

#include "Net/TlsStream.h"

#include <cstdio>
#include <stdexcept>
#include <string>

#include <esp_crt_bundle.h>
#include <esp_tls.h>
#include <mbedtls/error.h>
#include <sys/select.h>

namespace ClankerCleaner::Net
{
    namespace
    {
        // What esp-tls recorded about the last failure on the connection.
        std::string TlsError(esp_tls_t* tls)
        {
            esp_tls_error_handle_t errorHandle = nullptr;
            int tlsCode = 0;
            int certificateFlags = 0;
            if(esp_tls_get_error_handle(tls, &errorHandle) != ESP_OK || !errorHandle)
            {
                return "unknown error";
            }
            const esp_err_t lastError = esp_tls_get_and_clear_last_error(errorHandle, &tlsCode, &certificateFlags);

            char text[128];
            if(certificateFlags != 0)
            {
                // mbedTLS's MBEDTLS_X509_BADCERT_* bits.
                std::snprintf(text, sizeof(text), "certificate rejected (flags 0x%x)", static_cast<unsigned>(certificateFlags));
            }
            else if(tlsCode != 0)
            {
                // esp-tls records mbedTLS's (negative) codes as positive.
                const int mbedTlsCode = tlsCode > 0 ? -tlsCode : tlsCode;
                char reason[64];
                mbedtls_strerror(mbedTlsCode, reason, sizeof(reason));
                std::snprintf(text, sizeof(text), "%s (mbedTLS -0x%04x)", reason, static_cast<unsigned>(-mbedTlsCode));
            }
            else
            {
                std::snprintf(text, sizeof(text), "%s", esp_err_to_name(lastError));
            }
            return text;
        }
    }

    struct TlsStream::PlatformState
    {
        ~PlatformState() { Close(); }

        void Close()
        {
            if(Connection)
            {
                esp_tls_conn_destroy(Connection);
                Connection = nullptr;
            }
        }

        esp_tls_t* Connection = nullptr;
        std::string Endpoint;
    };

    TlsStream::TlsStream()
        : platform(std::make_unique<PlatformState>())
    {
    }

    TlsStream::~TlsStream() = default;

    void TlsStream::Connect(const std::string& host, int port, std::chrono::seconds timeout, std::string_view trustedCertificatesPem)
    {
        Close();
        PlatformState& state = *platform;
        state.Endpoint = host + ":" + std::to_string(port);

        // mbedTLS wants PEM text NUL-terminated, with the NUL counted.
        const std::string trustedCertificates(trustedCertificatesPem);
        esp_tls_cfg_t config{};
        // Also the socket's send and receive timeout.
        config.timeout_ms = static_cast<int>(std::chrono::milliseconds(timeout).count());
        if(trustedCertificates.empty())
        {
            config.crt_bundle_attach = esp_crt_bundle_attach;
        }
        else
        {
            config.cacert_buf = reinterpret_cast<const unsigned char*>(trustedCertificates.c_str());
            config.cacert_bytes = static_cast<unsigned int>(trustedCertificates.size() + 1);
        }

        state.Connection = esp_tls_init();
        if(!state.Connection)
        {
            throw std::runtime_error("TLS setup for " + state.Endpoint + " failed: out of memory");
        }
        // Checks the certificate against the host name and sends it for SNI.
        if(esp_tls_conn_new_sync(host.c_str(), static_cast<int>(host.size()), port, &config, state.Connection) != 1)
        {
            const std::string error = TlsError(state.Connection);
            Close();
            throw std::runtime_error("TLS connection to " + state.Endpoint + " failed: " + error);
        }
    }

    void TlsStream::Close()
    {
        platform->Close();
    }

    bool TlsStream::IsOpen() const
    {
        return platform->Connection != nullptr;
    }

    void TlsStream::Write(std::span<const std::uint8_t> data)
    {
        PlatformState& state = *platform;
        if(!state.Connection)
        {
            throw std::runtime_error("write on a closed TLS connection");
        }
        std::size_t written = 0;
        while(written < data.size())
        {
            const ssize_t result = esp_tls_conn_write(state.Connection, data.data() + written, data.size() - written);
            if(result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE)
            {
                continue;
            }
            if(result < 0)
            {
                throw std::runtime_error("write to " + state.Endpoint + " failed: " + TlsError(state.Connection));
            }
            written += static_cast<std::size_t>(result);
        }
    }

    std::size_t TlsStream::Read(std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout)
    {
        PlatformState& state = *platform;
        if(!state.Connection)
        {
            throw std::runtime_error("read on a closed TLS connection");
        }
        // Decrypted bytes may already be buffered inside mbedTLS, where select() can't see them.
        if(esp_tls_get_bytes_avail(state.Connection) <= 0)
        {
            int socketHandle = -1;
            esp_tls_get_conn_sockfd(state.Connection, &socketHandle);
            fd_set readable;
            FD_ZERO(&readable);
            FD_SET(socketHandle, &readable);
            timeval wait{};
            wait.tv_sec = static_cast<time_t>(timeout.count() / 1000);
            wait.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
            const int ready = ::select(socketHandle + 1, &readable, nullptr, nullptr, &wait);
            if(ready == 0)
            {
                return 0;
            }
            if(ready < 0)
            {
                throw std::runtime_error("waiting on " + state.Endpoint + " failed: select error");
            }
        }

        const ssize_t result = esp_tls_conn_read(state.Connection, buffer.data(), buffer.size());
        if(result > 0)
        {
            return static_cast<std::size_t>(result);
        }
        if(result == ESP_TLS_ERR_SSL_WANT_READ || result == ESP_TLS_ERR_SSL_WANT_WRITE)
        {
            // Only part of a TLS record, or a post-handshake message, arrived so far.
            return 0;
        }
        if(result == 0)
        {
            throw std::runtime_error(state.Endpoint + " closed the connection");
        }
        throw std::runtime_error("read from " + state.Endpoint + " failed: " + TlsError(state.Connection));
    }
}
