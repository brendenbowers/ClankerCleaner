// Placeholder until the cloud client is ported (Config.ESP32.cpp keeps [dreame] off): the
// stream never opens. The port would use esp-tls.

#include "Net/TlsStream.h"

#include <stdexcept>

namespace ClankerCleaner::Net
{
    struct TlsStream::PlatformState
    {
    };

    TlsStream::TlsStream()
        : platform(std::make_unique<PlatformState>())
    {
    }

    TlsStream::~TlsStream() = default;

    void TlsStream::Connect(const std::string& host, int port, std::chrono::seconds /*timeout*/, std::string_view /*trustedCertificatesPem*/)
    {
        throw std::runtime_error("TLS connection to " + host + ":" + std::to_string(port) + " failed: TLS isn't ported to the ESP32 yet");
    }

    void TlsStream::Close()
    {
    }

    bool TlsStream::IsOpen() const
    {
        return false;
    }

    void TlsStream::Write(std::span<const std::uint8_t> /*data*/)
    {
        throw std::runtime_error("TLS stream isn't open");
    }

    std::size_t TlsStream::Read(std::span<std::uint8_t> /*buffer*/, std::chrono::milliseconds /*timeout*/)
    {
        throw std::runtime_error("TLS stream isn't open");
    }
}
