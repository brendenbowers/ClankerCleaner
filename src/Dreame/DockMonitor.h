#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Dreame/CloudClient.h"
#include "Dreame/Types.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::Dreame
{
    // Polls the robot and its dock through the cloud on its own thread. States read the
    // latest snapshot with GetStatus(); nothing here blocks the state machine.
    class DockMonitor
    {
    public:
        // Called on the monitor's thread whenever a poll changes anything (including going
        // offline or back online). Keep it short, e.g. log and Machine::Wake().
        using ChangeListener = std::function<void(const DockStatus& previous, const DockStatus& current)>;

        DockMonitor(CloudOptions options, std::chrono::seconds pollInterval);
        ~DockMonitor();

        DockMonitor(const DockMonitor&) = delete;
        DockMonitor& operator=(const DockMonitor&) = delete;

        // Set before Start().
        void SetChangeListener(ChangeListener listener);

        void Start();
        void Stop();

        // Polls now instead of waiting for the next interval.
        void PollNow() { wakeEvent.Set(); }

        // Safe from any thread.
        DockStatus GetStatus() const;

    private:
        void Run(std::stop_token stopToken);
        void Publish(const DockStatus& next);

        CloudOptions options;
        std::chrono::seconds pollInterval;
        ChangeListener changeListener;

        mutable std::mutex statusMutex;
        DockStatus status;

        Threading::ManualResetEvent wakeEvent;
        std::jthread worker;
    };
}
