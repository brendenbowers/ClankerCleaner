#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace ClankerCleaner::Net
{
    // A blocking TLS connection to one host, with the certificate and host name verified.
    // Every failure throws std::runtime_error, including the peer closing the connection.
    class TlsStream
    {
    public:
        TlsStream();
        ~TlsStream();

        TlsStream(const TlsStream&) = delete;
        TlsStream& operator=(const TlsStream&) = delete;

        // trustedCertificatesPem replaces the system CA store when given, for servers on a
        // private CA.
        void Connect(const std::string& host, int port, std::chrono::seconds timeout, std::string_view trustedCertificatesPem = {});
        void Close();
        bool IsOpen() const;

        void Write(std::span<const std::uint8_t> data);

        // Reads whatever arrives within the timeout; returns 0 if nothing did.
        std::size_t Read(std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout);

    private:
        struct PlatformState;
        std::unique_ptr<PlatformState> platform;
    };
}
