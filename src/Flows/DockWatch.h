#pragma once

#include <atomic>

#include "Dreame/DockMonitor.h"
#include "Dreame/Types.h"

namespace ClankerCleaner::Flows
{
    // The robot's mop washing as a flow sees it. It's kept up to date by a dock monitor
    // listener for the flow's whole life, so any state can check it, not only the one that
    // happens to be listening. Safe from any thread.
    class DockWatch
    {
    public:
        // Starts following monitor's changes. The monitor must stop calling its listeners
        // before this is destroyed; main.cpp stops it before the machine and its flows go.
        void Watch(Dreame::DockMonitor& monitor);

        bool IsWashingMop() const { return washingMop; }

        // True once for each mop wash that finished without being interrupted since the
        // last call or ClearWashFinished.
        bool TakeWashFinished() { return washFinished.exchange(false); }
        void ClearWashFinished() { washFinished = false; }

    private:
        // Runs on a dock monitor thread.
        void HandleChange(const Dreame::DockStatus& previous, const Dreame::DockStatus& current);

        std::atomic<bool> washingMop{false};
        std::atomic<bool> washFinished{false};
    };
}
