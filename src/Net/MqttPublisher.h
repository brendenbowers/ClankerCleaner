#pragma once

#include <atomic>
#include <cstddef>
#include <deque>
#include <mutex>
#include <stop_token>
#include <string>
#include <thread>

#include "Config/Config.h"

namespace ClankerCleaner::Net
{
    // Keeps a connection to our MQTT broker on a background thread and publishes what's
    // queued, under the topic prefix. <prefix>/status is "online" (retained) while connected;
    // the broker sets it to "offline" if the connection drops. Reconnects with a backoff.
    class MqttPublisher
    {
    public:
        explicit MqttPublisher(Config::MqttSettings settings);
        ~MqttPublisher();

        MqttPublisher(const MqttPublisher&) = delete;
        MqttPublisher& operator=(const MqttPublisher&) = delete;

        void Start();
        void Stop();

        // Safe from any thread and never blocks. topic is relative to the prefix, e.g.
        // "dirty_tank/state". Sent at QoS 1; while disconnected, messages wait in a queue
        // that drops the oldest past MaxQueued.
        void Publish(const std::string& topic, std::string payload, bool retain = false);

        bool IsConnected() const { return connected; }

        static constexpr std::size_t MaxQueued = 100;

    private:
        struct Outgoing
        {
            std::string Topic;
            std::string Payload;
            bool Retain = false;
        };

        void Run(std::stop_token stopToken);
        std::string FullTopic(const std::string& topic) const;

        Config::MqttSettings settings;
        std::mutex queueMutex;
        std::deque<Outgoing> queue;
        std::atomic<bool> connected{false};
        std::jthread worker;
    };
}
