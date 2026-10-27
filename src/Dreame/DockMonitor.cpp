#include "Dreame/DockMonitor.h"

#include "Dreame/BrokerCertificates.h"
#include "Logging/Logging.h"
#include "Net/MqttClient.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <map>
#include <utility>
#include <vector>

namespace ClankerCleaner::Dreame
{
    using nlohmann::json;

    namespace
    {
        const std::vector<PropertyId> PolledProperties{
            Properties::RobotState, Properties::Error, Properties::RobotStatus, Properties::TaskStatus,
            Properties::BaseStatus, Properties::LowWaterWarning, Properties::CleanWaterTank, Properties::DirtyWaterTank,
            Properties::DetergentTank,
        };

        // After a failure, wait twice as long each time up to this.
        constexpr std::chrono::minutes MaxRetryDelay{15};
        // A rejected sign-in waits at least this long, so a wrong password doesn't lock the account.
        constexpr std::chrono::minutes AuthenticationRetryDelay{30};
        // While pushes arrive, poll only this often to catch anything they miss.
        constexpr std::chrono::minutes PushRefreshInterval{5};

        constexpr std::chrono::seconds FirstPushRetryDelay{10};
        constexpr std::chrono::minutes MaxPushRetryDelay{5};
        // How often the push thread checks for Stop() while waiting for messages.
        constexpr std::chrono::seconds PushReceiveSlice{1};
        // Will QoS 1 without a will, as the app's mqtt_client withWillQos() sends it.
        constexpr std::uint8_t AppConnectFlags = 0x08;

        void ApplyProperty(DockStatus& status, PropertyId property, std::int64_t value)
        {
            if (property == Properties::RobotState) status.Robot = static_cast<RobotState>(value);
            else if (property == Properties::RobotStatus) status.Status = static_cast<RobotStatus>(value);
            else if (property == Properties::TaskStatus) status.Task = static_cast<TaskStatus>(value);
            else if (property == Properties::BaseStatus) status.Base = static_cast<BaseStatus>(value);
            else if (property == Properties::CleanWaterTank) status.CleanTank = static_cast<CleanWaterTank>(value);
            else if (property == Properties::DirtyWaterTank) status.DirtyTank = static_cast<DirtyWaterTank>(value);
            else if (property == Properties::LowWaterWarning) status.LowWater = static_cast<LowWaterWarning>(value);
            else if (property == Properties::DetergentTank) status.Detergent = static_cast<DetergentTank>(value);
            else if (property == Properties::Error) status.ErrorCode = static_cast<std::int32_t>(value);
        }

        bool SameChannel(const PushChannel& left, const PushChannel& right)
        {
            return left.Host == right.Host && left.Port == right.Port && left.ClientId == right.ClientId &&
                   left.Username == right.Username && left.Password == right.Password && left.Topic == right.Topic;
        }
    }

    DockMonitor::DockMonitor(CloudOptions options, std::chrono::seconds pollInterval, bool usePush)
        : options(std::move(options))
        , pollInterval(pollInterval)
        , usePush(usePush)
    {
    }

    DockMonitor::~DockMonitor()
    {
        Stop();
    }

    DockMonitor::ListenerId DockMonitor::AddChangeListener(ChangeListener listener)
    {
        std::lock_guard<std::mutex> lock(listenerMutex);
        const ListenerId listenerId = nextListenerId++;
        changeListeners.emplace_back(listenerId, std::move(listener));
        return listenerId;
    }

    void DockMonitor::RemoveChangeListener(ListenerId listenerId)
    {
        std::lock_guard<std::mutex> lock(listenerMutex);
        std::erase_if(changeListeners, [listenerId](const auto& entry) { return entry.first == listenerId; });
    }

    void DockMonitor::Start()
    {
        if (!pollWorker.joinable())
        {
            pollWorker = std::jthread([this](std::stop_token stopToken) { RunPolling(stopToken); });
        }
        if (usePush && !pushWorker.joinable())
        {
            pushWorker = std::jthread([this](std::stop_token stopToken) { RunPush(stopToken); });
        }
    }

    void DockMonitor::Stop()
    {
        for (std::jthread* worker : {&pushWorker, &pollWorker})
        {
            if (worker->joinable())
            {
                worker->request_stop();
                worker->join();
            }
        }
    }

    DockStatus DockMonitor::GetStatus() const
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return status;
    }

    void DockMonitor::RunPolling(std::stop_token stopToken)
    {
        auto log = Logging::Get("Dreame");
        // Created here so its connection lives on this thread; nothing else touches it.
        CloudClient client(options);
        int failureCount = 0;

        while (!stopToken.stop_requested())
        {
            std::chrono::seconds delay = pushConnected ? std::max<std::chrono::seconds>(pollInterval, PushRefreshInterval) : pollInterval;
            try
            {
                if (pushNeedsNewToken.exchange(false))
                {
                    client.Login();
                }
                if (!client.GetSelectedDevice())
                {
                    const DeviceInfo& device = client.SelectDevice();
                    log->info("watching {} ({})", device.Name, device.Model);
                }

                const std::map<PropertyId, std::int64_t> values = client.GetProperties(PolledProperties);
                DockStatus next;
                next.Online = true;
                next.UpdatedAt = std::chrono::system_clock::now();
                for (const auto& [property, value] : values)
                {
                    ApplyProperty(next, property, value);
                }

                if (failureCount > 0)
                {
                    log->info("cloud reachable again");
                }
                failureCount = 0;
                Update([&next](DockStatus& current) { current = next; });

                if (usePush)
                {
                    OfferPushChannel(client.GetPushChannel());
                }
            }
            catch (const AuthenticationError& error)
            {
                ++failureCount;
                log->error("{}; check [dreame] username and password", error.what());
                delay = std::chrono::duration_cast<std::chrono::seconds>(AuthenticationRetryDelay);
            }
            catch (const std::exception& error)
            {
                ++failureCount;
                const auto backoff = pollInterval * (1 << std::min(failureCount - 1, 6));
                delay = std::min<std::chrono::seconds>(backoff, MaxRetryDelay);
                log->warn("poll failed ({} in a row, retrying in {} s): {}", failureCount, delay.count(), error.what());
            }

            if (failureCount > 0)
            {
                Update([](DockStatus& current) { current.Online = false; });
            }

            if (wakeEvent.WaitOne(delay, stopToken))
            {
                wakeEvent.Reset();
            }
        }
    }

    void DockMonitor::OfferPushChannel(PushChannel channel)
    {
        std::lock_guard<std::mutex> lock(pushChannelMutex);
        if (!pushChannel || !SameChannel(*pushChannel, channel))
        {
            pushChannel = std::move(channel);
            pushChannelChanged.Set();
        }
    }

    void DockMonitor::RunPush(std::stop_token stopToken)
    {
        auto log = Logging::Get("Dreame");
        Net::MqttClient mqtt;
        int failureCount = 0;

        while (!stopToken.stop_requested())
        {
            std::optional<PushChannel> channel;
            {
                std::lock_guard<std::mutex> lock(pushChannelMutex);
                channel = pushChannel;
                pushChannelChanged.Reset();
            }
            if (!channel)
            {
                // The poll thread offers one after its first successful poll.
                pushChannelChanged.WaitOne(stopToken);
                continue;
            }

            try
            {
                Net::MqttConnectOptions connectOptions;
                connectOptions.Host = channel->Host;
                connectOptions.Port = channel->Port;
                connectOptions.ClientId = channel->ClientId;
                connectOptions.Username = channel->Username;
                connectOptions.Password = channel->Password;
                connectOptions.Timeout = options.Timeout;
                connectOptions.ExtraConnectFlags = AppConnectFlags;
                connectOptions.TrustedCertificatesPem = std::string(BrokerCaCertificates);
                mqtt.Connect(connectOptions);
                mqtt.Subscribe(channel->Topic);

                pushConnected = true;
                failureCount = 0;
                log->info("push updates on ({}:{})", channel->Host, channel->Port);
                while (!stopToken.stop_requested())
                {
                    if (std::optional<Net::MqttMessage> message = mqtt.Receive(PushReceiveSlice))
                    {
                        HandlePush(message->Payload);
                    }
                }
                mqtt.Disconnect();
                pushConnected = false;
                return;
            }
            catch (const Net::MqttRefusedError& error)
            {
                // Usually an expired token; the poll thread fetches a new one and offers it.
                pushNeedsNewToken = true;
                wakeEvent.Set();
                ++failureCount;
                log->warn("push updates off: {}", error.what());
            }
            catch (const std::exception& error)
            {
                ++failureCount;
                log->warn("push updates off: {}", error.what());
            }

            pushConnected = false;
            // Poll at the normal rate again while push is down.
            wakeEvent.Set();
            const auto delay = std::min<std::chrono::seconds>(FirstPushRetryDelay * (1 << std::min(failureCount - 1, 6)), MaxPushRetryDelay);
            log->debug("retrying push in {} s", delay.count());
            // A new channel (fresh token) retries sooner.
            pushChannelChanged.WaitOne(delay, stopToken);
        }
    }

    void DockMonitor::HandlePush(const std::string& payload)
    {
        Logging::Get("Dreame")->trace("push: {}", payload);
        const json message = json::parse(payload, nullptr, false);
        if (!message.is_object() || !message.contains("data") || !message["data"].is_object())
        {
            return;
        }
        const json& data = message["data"];
        if (data.value("method", std::string()) != "properties_changed" || !data.contains("params") || !data["params"].is_array())
        {
            return;
        }

        std::vector<std::pair<PropertyId, std::int64_t>> changes;
        for (const json& parameter : data["params"])
        {
            if (!parameter.is_object() || !parameter.contains("siid") || !parameter.contains("piid") || !parameter.contains("value") ||
                !parameter["siid"].is_number_integer() || !parameter["piid"].is_number_integer())
            {
                continue;
            }
            const json& value = parameter["value"];
            if (!value.is_number_integer() && !value.is_boolean())
            {
                continue;
            }
            const PropertyId property{parameter["siid"].get<std::int32_t>(), parameter["piid"].get<std::int32_t>()};
            changes.emplace_back(property, value.is_boolean() ? (value.get<bool>() ? 1 : 0) : value.get<std::int64_t>());
        }
        if (changes.empty())
        {
            return;
        }

        Update([&changes](DockStatus& current)
        {
            current.Online = true;
            current.UpdatedAt = std::chrono::system_clock::now();
            for (const auto& [property, value] : changes)
            {
                ApplyProperty(current, property, value);
            }
        });
    }

    void DockMonitor::Update(const std::function<void(DockStatus&)>& change)
    {
        std::lock_guard<std::mutex> listenerLock(listenerMutex);
        DockStatus previous;
        DockStatus next;
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            previous = status;
            change(status);
            next = status;
        }
        if (previous.SameStateAs(next))
        {
            return;
        }
        for (const auto& entry : changeListeners)
        {
            entry.second(previous, next);
        }
    }
}
