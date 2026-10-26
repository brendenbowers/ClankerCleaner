#include "Net/MqttClient.h"

#include <algorithm>
#include <array>

namespace ClankerCleaner::Net
{
    namespace
    {
        enum PacketType : std::uint8_t
        {
            ConnectPacket = 1,
            ConnAckPacket = 2,
            PublishPacket = 3,
            PubAckPacket = 4,
            SubscribePacket = 8,
            SubAckPacket = 9,
            PingReqPacket = 12,
            PingRespPacket = 13,
            DisconnectPacket = 14,
        };

        constexpr std::uint8_t CleanSessionFlag = 0x02;
        constexpr std::uint8_t PasswordFlag = 0x40;
        constexpr std::uint8_t UsernameFlag = 0x80;
        constexpr std::size_t MaxPacketSize = 1024 * 1024;

        void AppendUint16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value >> 8));
            bytes.push_back(static_cast<std::uint8_t>(value & 0xFF));
        }

        void AppendString(std::vector<std::uint8_t>& bytes, const std::string& text)
        {
            AppendUint16(bytes, static_cast<std::uint16_t>(text.size()));
            bytes.insert(bytes.end(), text.begin(), text.end());
        }

        std::uint16_t ReadUint16(const std::vector<std::uint8_t>& bytes, std::size_t offset)
        {
            return static_cast<std::uint16_t>((bytes.at(offset) << 8) | bytes.at(offset + 1));
        }

        std::string ConnAckReason(int returnCode)
        {
            switch (returnCode)
            {
            case 1: return "unsupported protocol version";
            case 2: return "client ID rejected";
            case 3: return "server unavailable";
            case 4: return "bad username or password";
            case 5: return "not authorized";
            }
            return "return code " + std::to_string(returnCode);
        }
    }

    void MqttClient::Connect(const MqttConnectOptions& options)
    {
        Disconnect();
        keepAlive = options.KeepAlive;
        timeout = options.Timeout;
        pingSentAt.reset();
        stream.Connect(options.Host, options.Port, options.Timeout, options.TrustedCertificatesPem);

        std::vector<std::uint8_t> body;
        AppendString(body, "MQTT");
        body.push_back(4); // protocol level 3.1.1
        std::uint8_t flags = CleanSessionFlag | options.ExtraConnectFlags;
        if (!options.Username.empty())
        {
            flags |= UsernameFlag;
        }
        if (!options.Password.empty())
        {
            flags |= PasswordFlag;
        }
        body.push_back(flags);
        AppendUint16(body, static_cast<std::uint16_t>(keepAlive.count()));
        AppendString(body, options.ClientId);
        if (!options.Username.empty())
        {
            AppendString(body, options.Username);
        }
        if (!options.Password.empty())
        {
            AppendString(body, options.Password);
        }
        Send(ConnectPacket << 4, body);

        const std::optional<Packet> reply = ReadPacket(std::chrono::steady_clock::now() + timeout);
        if (!reply || (reply->Header >> 4) != ConnAckPacket || reply->Body.size() < 2)
        {
            Fail("no CONNACK from " + options.Host);
        }
        const int returnCode = reply->Body[1];
        if (returnCode != 0)
        {
            Disconnect();
            throw MqttRefusedError(returnCode, options.Host + " refused the connection: " + ConnAckReason(returnCode));
        }
    }

    void MqttClient::Disconnect()
    {
        if (stream.IsOpen())
        {
            try
            {
                Send(DisconnectPacket << 4, {});
            }
            catch (const std::exception&)
            {
                // Closing anyway.
            }
            stream.Close();
        }
        received.clear();
    }

    void MqttClient::Subscribe(const std::string& topic, std::uint8_t qos)
    {
        const std::uint16_t packetId = nextPacketId++;
        if (nextPacketId == 0)
        {
            nextPacketId = 1;
        }
        std::vector<std::uint8_t> body;
        AppendUint16(body, packetId);
        AppendString(body, topic);
        body.push_back(std::min<std::uint8_t>(qos, 1));
        Send((SubscribePacket << 4) | 0x02, body);

        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true)
        {
            const std::optional<Packet> packet = ReadPacket(deadline);
            if (!packet)
            {
                Fail("no SUBACK for " + topic);
            }
            if ((packet->Header >> 4) == SubAckPacket && packet->Body.size() >= 3 && ReadUint16(packet->Body, 0) == packetId)
            {
                if (packet->Body[2] == 0x80)
                {
                    Fail("subscription to " + topic + " refused");
                }
                return;
            }
            // A message can arrive before the SUBACK; dropping it is fine for a status feed.
            HandlePacket(*packet);
        }
    }

    std::optional<MqttMessage> MqttClient::Receive(std::chrono::milliseconds waitTime)
    {
        const auto deadline = std::chrono::steady_clock::now() + waitTime;
        while (true)
        {
            const auto now = std::chrono::steady_clock::now();
            if (pingSentAt && now - *pingSentAt > timeout)
            {
                Fail("no PINGRESP within " + std::to_string(timeout.count()) + " s");
            }
            // Ping a little before the keep-alive runs out, as the broker drops us at 1.5x.
            const auto pingDue = lastSent + keepAlive * 3 / 4;
            if (!pingSentAt && now >= pingDue)
            {
                Send(PingReqPacket << 4, {});
                pingSentAt = now;
            }
            if (now >= deadline)
            {
                return std::nullopt;
            }

            auto wakeAt = std::min(deadline, pingSentAt ? *pingSentAt + timeout : lastSent + keepAlive * 3 / 4);
            const std::optional<Packet> packet = ReadPacket(std::max(wakeAt, now));
            if (packet)
            {
                if (std::optional<MqttMessage> message = HandlePacket(*packet))
                {
                    return message;
                }
            }
        }
    }

    void MqttClient::Send(std::uint8_t header, const std::vector<std::uint8_t>& body)
    {
        std::vector<std::uint8_t> packet{header};
        std::size_t remaining = body.size();
        do
        {
            std::uint8_t encoded = remaining % 128;
            remaining /= 128;
            if (remaining > 0)
            {
                encoded |= 0x80;
            }
            packet.push_back(encoded);
        } while (remaining > 0);
        packet.insert(packet.end(), body.begin(), body.end());

        try
        {
            stream.Write(packet);
        }
        catch (const std::exception& error)
        {
            Fail(error.what());
        }
        lastSent = std::chrono::steady_clock::now();
    }

    std::optional<MqttClient::Packet> MqttClient::ReadPacket(std::chrono::steady_clock::time_point deadline)
    {
        while (true)
        {
            // Try to parse a whole packet from what's buffered.
            if (received.size() >= 2)
            {
                std::size_t remaining = 0;
                std::size_t multiplier = 1;
                std::size_t headerLength = 1;
                bool lengthComplete = false;
                while (headerLength < received.size() && headerLength <= 4)
                {
                    const std::uint8_t encoded = received[headerLength++];
                    remaining += (encoded & 0x7F) * multiplier;
                    multiplier *= 128;
                    if ((encoded & 0x80) == 0)
                    {
                        lengthComplete = true;
                        break;
                    }
                }
                if (!lengthComplete && headerLength > 4)
                {
                    Fail("malformed packet length");
                }
                if (lengthComplete && remaining > MaxPacketSize)
                {
                    Fail("packet of " + std::to_string(remaining) + " bytes is too large");
                }
                if (lengthComplete && received.size() >= headerLength + remaining)
                {
                    Packet packet;
                    packet.Header = received[0];
                    packet.Body.assign(received.begin() + static_cast<std::ptrdiff_t>(headerLength),
                                       received.begin() + static_cast<std::ptrdiff_t>(headerLength + remaining));
                    received.erase(received.begin(), received.begin() + static_cast<std::ptrdiff_t>(headerLength + remaining));
                    return packet;
                }
            }

            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
            {
                return std::nullopt;
            }
            std::array<std::uint8_t, 4096> chunk{};
            std::size_t count = 0;
            try
            {
                count = stream.Read(chunk, std::chrono::ceil<std::chrono::milliseconds>(deadline - now));
            }
            catch (const std::exception& error)
            {
                Fail(error.what());
            }
            received.insert(received.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(count));
        }
    }

    std::optional<MqttMessage> MqttClient::HandlePacket(const Packet& packet)
    {
        switch (packet.Header >> 4)
        {
        case PingRespPacket:
            pingSentAt.reset();
            return std::nullopt;
        case PublishPacket:
        {
            const int qos = (packet.Header >> 1) & 0x03;
            const std::size_t topicLength = ReadUint16(packet.Body, 0);
            std::size_t offset = 2 + topicLength;
            if (offset > packet.Body.size())
            {
                Fail("malformed PUBLISH");
            }
            MqttMessage message;
            message.Topic.assign(packet.Body.begin() + 2, packet.Body.begin() + static_cast<std::ptrdiff_t>(offset));
            if (qos > 0)
            {
                const std::uint16_t packetId = ReadUint16(packet.Body, offset);
                offset += 2;
                std::vector<std::uint8_t> acknowledgement;
                AppendUint16(acknowledgement, packetId);
                Send(PubAckPacket << 4, acknowledgement);
            }
            message.Payload.assign(packet.Body.begin() + static_cast<std::ptrdiff_t>(offset), packet.Body.end());
            return message;
        }
        default:
            return std::nullopt;
        }
    }

    void MqttClient::Fail(const std::string& message)
    {
        stream.Close();
        received.clear();
        throw std::runtime_error(message);
    }
}
