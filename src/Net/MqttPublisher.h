#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

#include "Config/Config.h"
#include "Net/MqttClient.h"

namespace ClankerCleaner::Net
{
    // Keeps a connection to our MQTT broker on a background thread and publishes what's
    // queued, under the topic prefix. <prefix>/status is "online" (retained) while connected;
    // the broker sets it to "offline" if the connection drops. Reconnects with a backoff.
    // Also receives from the topics given to Subscribe.
    class MqttPublisher
    {
    public:
        explicit MqttPublisher(Config::MqttSettings settings);
        ~MqttPublisher();

        MqttPublisher(const MqttPublisher&) = delete;
        MqttPublisher& operator=(const MqttPublisher&) = delete;

        // Before Start. topic is relative to the prefix and may use wildcards, e.g. "commands/#".
        // Subscribed again on each reconnect, at QoS 1.
        void Subscribe(std::string topic);

        void Start();
        void Stop();

        // Safe from any thread and never blocks. topic is relative to the prefix, e.g.
        // "dirty_tank/state". Sent at QoS 1; while disconnected, messages wait in a queue
        // that drops the oldest past MaxQueued.
        void Publish(const std::string& topic, std::string payload, bool retain = false);

        // Safe from any thread and never blocks. The oldest message received and not yet taken,
        // its topic relative to the prefix. Past MaxReceived the oldest is dropped. Retained
        // messages are ignored, so nothing stored on the broker is acted on after a reconnect.
        std::optional<MqttMessage> TakeReceived();

        bool IsConnected() const { return connected; }

        static constexpr std::size_t MaxQueued = 100;
        static constexpr std::size_t MaxReceived = 8;

    private:
        struct Outgoing
        {
            std::string Topic;
            std::string Payload;
            bool Retain = false;
        };

        void Run(std::stop_token stopToken);
        std::string FullTopic(const std::string& topic) const;
        void StoreReceived(MqttMessage message);

        Config::MqttSettings settings;
        std::mutex queueMutex;
        std::deque<Outgoing> queue;
        std::vector<std::string> subscriptions;
        std::mutex receivedMutex;
        std::deque<MqttMessage> received;
        std::atomic<bool> connected{false};
        std::jthread worker;
    };
}
