#pragma once

#include <atomic>
#include <chrono>
#include <limits>
#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/DirtyTank/DirtyTank.h"


#include "Dreame/Types.h"
#include "Dreame/DockMonitor.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class Idle : public StateWithFlow<DirtyTankFlow>
    {
    public:
        Idle();
        ~Idle() override = default;

        Transition Begin(DirtyTankFlow& flow) override;
        Transition Run(DirtyTankFlow& flow) override;
        bool End(DirtyTankFlow& flow) override;
        void Abort(DirtyTankFlow& flow) override;
    private:
        void HandleDockStatusChanged(const Dreame::DockStatus& previous, const Dreame::DockStatus& current);
        void UnregisterDockStatusChanged(DirtyTankFlow& flow);

        static constexpr Dreame::DockMonitor::ListenerId NoListener = std::numeric_limits<Dreame::DockMonitor::ListenerId>::max();

        // Set by the dock monitor thread, consumed by Run.
        std::atomic<bool> mopWashFinished{false};
        Dreame::DockMonitor::ListenerId DockStatusChangedEventId = NoListener;
        std::uint64_t lastEvent;
    };
}