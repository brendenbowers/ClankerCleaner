#include "Net/MqttPublisher.h"

#include <algorithm>
#include <chrono>
#include <exception>
#include <optional>
#include <utility>

#include "Logging/Logging.h"
#include "Net/MqttClient.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::Net
{
    namespace
    {
        // How long Run waits for incoming packets (pings) before checking the queue again,
        // so it's also the most a queued message waits.
        constexpr auto ReceiveSlice = std::chrono::milliseconds(250);
        constexpr auto FirstRetryDelay = std::chrono::seconds(5);
        constexpr auto MaxRetryDelay = std::chrono::seconds(300);
    }

    MqttPublisher::MqttPublisher(Config::MqttSettings settings)
        : settings(std::move(settings))
    {
    }

    MqttPublisher::~MqttPublisher()
    {
        Stop();
    }

    void MqttPublisher::Start()
    {
        if (!worker.joinable())
        {
            worker = std::jthread([this](std::stop_token stopToken) { Run(stopToken); });
        }
    }

    void MqttPublisher::Stop()
    {
        if (worker.joinable())
        {
            worker.request_stop();
            worker.join();
        }
    }

    void MqttPublisher::Publish(const std::string& topic, std::string payload, bool retain)
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        if (queue.size() >= MaxQueued)
        {
            queue.pop_front();
        }
        queue.push_back(Outgoing{topic, std::move(payload), retain});
    }

    std::string MqttPublisher::FullTopic(const std::string& topic) const
    {
        return settings.TopicPrefix.empty() ? topic : settings.TopicPrefix + "/" + topic;
    }

    void MqttPublisher::Run(std::stop_token stopToken)
    {
        auto log = Logging::Get("Mqtt");
        const std::string statusTopic = FullTopic("status");
        MqttClient mqtt;
        int failureCount = 0;
        // Never set: only waited on for the retry delay, which the stop token cuts short.
        Threading::ManualResetEvent retryWait;

        while (!stopToken.stop_requested())
        {
            try
            {
                MqttConnectOptions options;
                options.Host = settings.Host;
                options.Port = settings.Port;
                options.ClientId = settings.ClientId;
                options.Username = settings.Username;
                options.Password = settings.Password;
                options.TrustedCertificatesPem = settings.CaCertificatesPem;
                options.WillTopic = statusTopic;
                options.WillPayload = "offline";
                options.WillQos = 1;
                options.WillRetain = true;
                mqtt.Connect(options);
                mqtt.Publish(statusTopic, "online", 1, true);

                connected = true;
                failureCount = 0;
                log->info("connected to {}:{}", settings.Host, settings.Port);
                while (!stopToken.stop_requested())
                {
                    std::optional<Outgoing> next;
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        if (!queue.empty())
                        {
                            next = std::move(queue.front());
                            queue.pop_front();
                        }
                    }
                    if (!next)
                    {
                        // Nothing is subscribed, so this only answers the keep-alive pings.
                        mqtt.Receive(ReceiveSlice);
                        continue;
                    }
                    try
                    {
                        mqtt.Publish(FullTopic(next->Topic), next->Payload, 1, next->Retain);
                    }
                    catch (const std::exception&)
                    {
                        // Send it again after reconnecting.
                        std::lock_guard<std::mutex> lock(queueMutex);
                        queue.push_front(std::move(*next));
                        throw;
                    }
                }

                mqtt.Publish(statusTopic, "offline", 1, true);
                mqtt.Disconnect();
                connected = false;
                return;
            }
            catch (const std::exception& error)
            {
                log->warn("not connected: {}", error.what());
            }

            connected = false;
            ++failureCount;
            const auto delay = std::min<std::chrono::seconds>(FirstRetryDelay * (1 << std::min(failureCount - 1, 6)), MaxRetryDelay);
            log->debug("retrying in {} s", delay.count());
            retryWait.WaitOne(delay, stopToken);
        }
    }
}
