#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "Net/TlsStream.h"

// A minimal MQTT 3.1.1 client over TLS: connect, subscribe, receive and publish, at QoS 0
// or 1. Enough for the Dreame/MOVA status push and our own broker; QoS 2 isn't supported.
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
        // Published by the broker if the connection drops without a DISCONNECT; no will
        // when the topic is empty.
        std::string WillTopic;
        std::string WillPayload;
        std::uint8_t WillQos = 0;
        bool WillRetain = false;
    };

    struct MqttMessage
    {
        std::string Topic;
        std::string Payload;
        // The broker's stored copy, sent because we subscribed, rather than a new message.
        bool Retained = false;
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

        // At QoS 1, waits for the PUBACK. QoS 2 is sent as 1. Messages that arrive while
        // waiting, here or in Subscribe, are held for Receive.
        void Publish(const std::string& topic, const std::string& payload, std::uint8_t qos = 0, bool retain = false);

        // Returns the next message, or nothing if none arrives within the timeout. Sends
        // keep-alive pings while waiting. A timeout of zero only returns what's already held.
        std::optional<MqttMessage> Receive(std::chrono::milliseconds timeout);

    private:
        struct Packet
        {
            std::uint8_t Header = 0;
            std::vector<std::uint8_t> Body;
        };

        std::uint16_t NextPacketId();
        void Send(std::uint8_t header, const std::vector<std::uint8_t>& body);
        // Reads until a whole packet is buffered or the deadline passes.
        std::optional<Packet> ReadPacket(std::chrono::steady_clock::time_point deadline);
        // Handles pings and acks; returns a message if the packet was a PUBLISH.
        std::optional<MqttMessage> HandlePacket(const Packet& packet);
        // HandlePacket for a packet read while waiting for an ack; keeps any message for Receive.
        void HoldMessage(const Packet& packet);
        void Fail(const std::string& message);

        TlsStream stream;
        std::vector<std::uint8_t> received;
        std::vector<std::uint8_t> readBuffer;
        std::deque<MqttMessage> held;
        std::chrono::seconds keepAlive{60};
        std::chrono::seconds timeout{10};
        std::chrono::steady_clock::time_point lastSent{};
        std::optional<std::chrono::steady_clock::time_point> pingSentAt;
        std::uint16_t nextPacketId = 1;
    };
}
