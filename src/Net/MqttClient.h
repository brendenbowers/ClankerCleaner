#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Net/TlsStream.h"

// A minimal MQTT 3.1.1 subscriber over TLS: connect, subscribe, receive. Enough for the
// Dreame/MOVA status push; publishing and QoS 2 aren't supported.
namespace ClankerCleaner::Net
{
    struct MqttConnectOptions
    {
        std::string Host;
        int Port = 8883;
        std::string ClientId;
        std::string Username;
        std::string Password;
        std::chrono::seconds KeepAlive{60};
        // Bounds connecting and waiting for CONNACK, SUBACK and PINGRESP.
        std::chrono::seconds Timeout{10};
        // OR'd into the CONNECT flags byte (clean session is always set).
        std::uint8_t ExtraConnectFlags = 0;
        // CA certificates to trust instead of the system store; empty uses the system store.
        std::string TrustedCertificatesPem;
    };

    struct MqttMessage
    {
        std::string Topic;
        std::string Payload;
    };

    // The broker answered CONNACK with a non-zero return code, e.g. 4 (bad username or
    // password) or 5 (not authorized).
    class MqttRefusedError : public std::runtime_error
    {
    public:
        MqttRefusedError(int returnCode, const std::string& message)
            : std::runtime_error(message)
            , ReturnCode(returnCode)
        {
        }

        int ReturnCode;
    };

    // Not thread-safe: use it from one thread. Failures throw std::runtime_error and leave
    // the client disconnected.
    class MqttClient
    {
    public:
        void Connect(const MqttConnectOptions& options);
        void Disconnect();
        bool IsConnected() const { return stream.IsOpen(); }

        // Waits for the SUBACK; throws if the broker refuses the topic.
        void Subscribe(const std::string& topic, std::uint8_t qos = 0);

        // Returns the next message, or nothing if none arrives within the timeout. Sends
        // keep-alive pings while waiting.
        std::optional<MqttMessage> Receive(std::chrono::milliseconds timeout);

    private:
        struct Packet
        {
            std::uint8_t Header = 0;
            std::vector<std::uint8_t> Body;
        };

        void Send(std::uint8_t header, const std::vector<std::uint8_t>& body);
        // Reads until a whole packet is buffered or the deadline passes.
        std::optional<Packet> ReadPacket(std::chrono::steady_clock::time_point deadline);
        // Handles pings and acks; returns a message if the packet was a PUBLISH.
        std::optional<MqttMessage> HandlePacket(const Packet& packet);
        void Fail(const std::string& message);

        TlsStream stream;
        std::vector<std::uint8_t> received;
        std::chrono::seconds keepAlive{60};
        std::chrono::seconds timeout{10};
        std::chrono::steady_clock::time_point lastSent{};
        std::optional<std::chrono::steady_clock::time_point> pingSentAt;
        std::uint16_t nextPacketId = 1;
    };
}
