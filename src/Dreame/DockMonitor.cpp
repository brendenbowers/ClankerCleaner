#include "Dreame/DockMonitor.h"

#include "Logging/Logging.h"

#include <algorithm>
#include <exception>
#include <map>
#include <vector>

namespace ClankerCleaner::Dreame
{
    namespace
    {
        const std::vector<PropertyId> PolledProperties{
            Properties::RobotState, Properties::Error, Properties::RobotStatus, Properties::TaskStatus,
            Properties::BaseStatus, Properties::LowWaterWarning, Properties::CleanWaterTank, Properties::DirtyWaterTank,
        };

        // After a failure, wait twice as long each time up to this.
        constexpr std::chrono::minutes MaxRetryDelay{15};
        // A rejected sign-in waits at least this long, so a wrong password doesn't lock the account.
        constexpr std::chrono::minutes AuthenticationRetryDelay{30};

        template <typename TEnum>
        TEnum ValueOf(const std::map<PropertyId, std::int64_t>& values, PropertyId property)
        {
            auto entry = values.find(property);
            return entry == values.end() ? TEnum(-1) : static_cast<TEnum>(entry->second);
        }
    }

    DockMonitor::DockMonitor(CloudOptions options, std::chrono::seconds pollInterval)
        : options(std::move(options))
        , pollInterval(pollInterval)
    {
    }

    DockMonitor::~DockMonitor()
    {
        Stop();
    }

    void DockMonitor::SetChangeListener(ChangeListener listener)
    {
        changeListener = std::move(listener);
    }

    void DockMonitor::Start()
    {
        if (!worker.joinable())
        {
            worker = std::jthread([this](std::stop_token stopToken) { Run(stopToken); });
        }
    }

    void DockMonitor::Stop()
    {
        if (worker.joinable())
        {
            worker.request_stop();
            worker.join();
        }
    }

    DockStatus DockMonitor::GetStatus() const
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return status;
    }

    void DockMonitor::Run(std::stop_token stopToken)
    {
        auto log = Logging::Get("Dreame");
        // Created here so its connection lives on this thread; nothing else touches it.
        CloudClient client(options);
        int failureCount = 0;

        while (!stopToken.stop_requested())
        {
            std::chrono::seconds delay = pollInterval;
            try
            {
                if (!client.GetSelectedDevice())
                {
                    const DeviceInfo& device = client.SelectDevice();
                    log->info("watching {} ({})", device.Name, device.Model);
                }

                const std::map<PropertyId, std::int64_t> values = client.GetProperties(PolledProperties);
                DockStatus next;
                next.Online = true;
                next.UpdatedAt = std::chrono::system_clock::now();
                next.Robot = ValueOf<RobotState>(values, Properties::RobotState);
                next.Status = ValueOf<RobotStatus>(values, Properties::RobotStatus);
                next.Task = ValueOf<TaskStatus>(values, Properties::TaskStatus);
                next.Base = ValueOf<BaseStatus>(values, Properties::BaseStatus);
                next.CleanTank = ValueOf<CleanWaterTank>(values, Properties::CleanWaterTank);
                next.DirtyTank = ValueOf<DirtyWaterTank>(values, Properties::DirtyWaterTank);
                next.LowWater = ValueOf<LowWaterWarning>(values, Properties::LowWaterWarning);
                auto error = values.find(Properties::Error);
                next.ErrorCode = error == values.end() ? -1 : static_cast<std::int32_t>(error->second);

                if (failureCount > 0)
                {
                    log->info("cloud reachable again");
                }
                failureCount = 0;
                Publish(next);
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
                DockStatus offline = GetStatus();
                offline.Online = false;
                Publish(offline);
            }

            if (wakeEvent.WaitOne(delay, stopToken))
            {
                wakeEvent.Reset();
            }
        }
    }

    void DockMonitor::Publish(const DockStatus& next)
    {
        DockStatus previous;
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            previous = status;
            status = next;
        }
        if (changeListener && !previous.SameStateAs(next))
        {
            changeListener(previous, next);
        }
    }
}
