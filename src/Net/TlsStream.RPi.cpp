#include "Net/TlsStream.h"

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>

#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace ClankerCleaner::Net
{
    namespace
    {
        constexpr const char* CaCertificatesPath = "/etc/ssl/certs/ca-certificates.crt";

        std::string OpenSslError()
        {
            const unsigned long code = ERR_get_error();
            if (code == 0)
            {
                return errno != 0 ? std::strerror(errno) : "unknown error";
            }
            char text[256];
            ERR_error_string_n(code, text, sizeof(text));
            return text;
        }

        // Adds every certificate in the PEM text to the context's trust store.
        bool AddTrustedCertificates(SSL_CTX* context, std::string_view pemText)
        {
            BIO* source = BIO_new_mem_buf(pemText.data(), static_cast<int>(pemText.size()));
            if (!source)
            {
                return false;
            }
            X509_STORE* store = SSL_CTX_get_cert_store(context);
            int added = 0;
            while (X509* certificate = PEM_read_bio_X509(source, nullptr, nullptr, nullptr))
            {
                added += X509_STORE_add_cert(store, certificate) == 1 ? 1 : 0;
                X509_free(certificate);
            }
            BIO_free(source);
            // Reading stops with a "no start line" error at the end of the text.
            ERR_clear_error();
            return added > 0;
        }

        void SetSocketTimeouts(int socketHandle, std::chrono::seconds timeout)
        {
            timeval value{};
            value.tv_sec = static_cast<time_t>(timeout.count());
            setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO, &value, sizeof(value));
            // On Linux this also bounds connect().
            setsockopt(socketHandle, SOL_SOCKET, SO_SNDTIMEO, &value, sizeof(value));
        }
    }

    struct TlsStream::PlatformState
    {
        ~PlatformState() { Close(); }

        void Close()
        {
            if (Connection)
            {
                SSL_shutdown(Connection);
                SSL_free(Connection);
                Connection = nullptr;
            }
            if (Context)
            {
                SSL_CTX_free(Context);
                Context = nullptr;
            }
            if (SocketHandle >= 0)
            {
                ::close(SocketHandle);
                SocketHandle = -1;
            }
        }

        int SocketHandle = -1;
        SSL_CTX* Context = nullptr;
        SSL* Connection = nullptr;
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

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo* addresses = nullptr;
        const int lookupResult = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses);
        if (lookupResult != 0)
        {
            throw std::runtime_error("can't resolve " + host + ": " + gai_strerror(lookupResult));
        }

        std::string lastError = "no addresses";
        for (const addrinfo* address = addresses; address && state.SocketHandle < 0; address = address->ai_next)
        {
            const int socketHandle = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
            if (socketHandle < 0)
            {
                lastError = std::strerror(errno);
                continue;
            }
            SetSocketTimeouts(socketHandle, timeout);
            if (::connect(socketHandle, address->ai_addr, address->ai_addrlen) == 0)
            {
                state.SocketHandle = socketHandle;
            }
            else
            {
                lastError = std::strerror(errno);
                ::close(socketHandle);
            }
        }
        freeaddrinfo(addresses);
        if (state.SocketHandle < 0)
        {
            throw std::runtime_error("can't connect to " + state.Endpoint + ": " + lastError);
        }

        state.Context = SSL_CTX_new(TLS_client_method());
        const bool trustLoaded = state.Context && (trustedCertificatesPem.empty()
                                                       ? SSL_CTX_load_verify_locations(state.Context, CaCertificatesPath, nullptr) == 1
                                                       : AddTrustedCertificates(state.Context, trustedCertificatesPem));
        if (!trustLoaded)
        {
            const std::string error = OpenSslError();
            Close();
            throw std::runtime_error("TLS setup failed: " + error);
        }
        SSL_CTX_set_verify(state.Context, SSL_VERIFY_PEER, nullptr);

        state.Connection = SSL_new(state.Context);
        SSL_set_fd(state.Connection, state.SocketHandle);
        SSL_set_tlsext_host_name(state.Connection, host.c_str());
        SSL_set1_host(state.Connection, host.c_str());
        ERR_clear_error();
        if (SSL_connect(state.Connection) != 1)
        {
            const long verifyResult = SSL_get_verify_result(state.Connection);
            std::string error = verifyResult != X509_V_OK ? std::string("certificate rejected: ") + X509_verify_cert_error_string(verifyResult)
                                                          : OpenSslError();
            Close();
            throw std::runtime_error("TLS handshake with " + host + ":" + std::to_string(port) + " failed: " + error);
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
        if (!state.Connection)
        {
            throw std::runtime_error("write on a closed TLS connection");
        }
        std::size_t written = 0;
        while (written < data.size())
        {
            ERR_clear_error();
            const int result = SSL_write(state.Connection, data.data() + written, static_cast<int>(data.size() - written));
            if (result <= 0)
            {
                throw std::runtime_error("write to " + state.Endpoint + " failed: " + OpenSslError());
            }
            written += static_cast<std::size_t>(result);
        }
    }

    std::size_t TlsStream::Read(std::span<std::uint8_t> buffer, std::chrono::milliseconds timeout)
    {
        PlatformState& state = *platform;
        if (!state.Connection)
        {
            throw std::runtime_error("read on a closed TLS connection");
        }
        // Decrypted bytes may already be buffered inside OpenSSL, where poll() can't see them.
        if (SSL_pending(state.Connection) == 0)
        {
            pollfd watched{state.SocketHandle, POLLIN, 0};
            const int ready = ::poll(&watched, 1, static_cast<int>(timeout.count()));
            if (ready == 0 || (ready < 0 && errno == EINTR))
            {
                return 0;
            }
            if (ready < 0)
            {
                throw std::runtime_error("waiting on " + state.Endpoint + " failed: " + std::strerror(errno));
            }
        }

        ERR_clear_error();
        const int result = SSL_read(state.Connection, buffer.data(), static_cast<int>(buffer.size()));
        if (result > 0)
        {
            return static_cast<std::size_t>(result);
        }
        const int error = SSL_get_error(state.Connection, result);
        if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ||
            (error == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK)))
        {
            // Only part of a TLS record, or a post-handshake message, arrived so far.
            return 0;
        }
        if (error == SSL_ERROR_ZERO_RETURN || (error == SSL_ERROR_SYSCALL && result == 0))
        {
            throw std::runtime_error(state.Endpoint + " closed the connection");
        }
        throw std::runtime_error("read from " + state.Endpoint + " failed: " + OpenSslError());
    }
}
