#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Dreame/CloudClient.h"
#include "Dreame/Types.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::Dreame
{
    // Watches the robot and its dock through the cloud on background threads: changes are
    // pushed over MQTT as the app gets them, with polling as the fallback and a periodic
    // full refresh. States read the latest snapshot with GetStatus(); nothing here blocks
    // the state machine.
    class DockMonitor
    {
    public:
        // Called on a monitor thread whenever anything changes (including going offline or
        // back online). Calls never overlap. Keep it short, e.g. log and Machine::Wake().
        using ChangeListener = std::function<void(const DockStatus& previous, const DockStatus& current)>;
        using ListenerId = std::uint64_t;

        // pollInterval applies while push updates are off; with them on, polling slows to a
        // refresh every few minutes.
        DockMonitor(CloudOptions options, std::chrono::seconds pollInterval, bool usePush);
        ~DockMonitor();

        DockMonitor(const DockMonitor&) = delete;
        DockMonitor& operator=(const DockMonitor&) = delete;

        // Listeners run in the order added. Safe from any thread, but not from inside a
        // listener (it would deadlock).
        ListenerId AddChangeListener(ChangeListener listener);
        void RemoveChangeListener(ListenerId listenerId);

        void Start();
        void Stop();

        // Polls now instead of waiting for the next interval.
        void PollNow() { wakeEvent.Set(); }

        // Safe from any thread.
        DockStatus GetStatus() const;
        bool IsPushConnected() const { return pushConnected; }

    private:
        void RunPolling(std::stop_token stopToken);
        void RunPush(std::stop_token stopToken);
        // Stores the channel and wakes the push thread if it changed (e.g. a new token).
        void OfferPushChannel(PushChannel channel);
        void HandlePush(const std::string& payload);
        // Applies a change to the status under the lock, then tells the listeners.
        void Update(const std::function<void(DockStatus&)>& change);

        CloudOptions options;
        std::chrono::seconds pollInterval;
        bool usePush;

        mutable std::mutex statusMutex;
        DockStatus status;
        // Guards the listener list and keeps calls from the two threads in order.
        std::mutex listenerMutex;
        std::vector<std::pair<ListenerId, ChangeListener>> changeListeners;
        ListenerId nextListenerId = 1;

        std::mutex pushChannelMutex;
        std::optional<PushChannel> pushChannel;
        Threading::ManualResetEvent pushChannelChanged;
        std::atomic<bool> pushConnected{false};
        // Set by the push thread when the broker refuses the token; the poll thread signs in again.
        std::atomic<bool> pushNeedsNewToken{false};

        Threading::ManualResetEvent wakeEvent;
        std::jthread pollWorker;
        std::jthread pushWorker;
    };
}
