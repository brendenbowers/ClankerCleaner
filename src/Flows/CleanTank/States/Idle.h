#pragma once


#include <atomic>
#include <chrono>
#include <limits>
#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/CleanTank/CleanTank.h"


#include "Dreame/Types.h"
#include "Dreame/DockMonitor.h"

namespace ClankerCleaner::Flows::CleanTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class Idle : public StateWithFlow<CleanTankFlow>
    {
    public:
        Idle();
        ~Idle() override = default;

        Transition Begin(CleanTankFlow& flow) override;
        Transition Run(CleanTankFlow& flow) override;
        bool End(CleanTankFlow& flow) override;
        void Abort(CleanTankFlow& flow) override;
    private:
        void HandleDockStatusChanged(const Dreame::DockStatus& previous, const Dreame::DockStatus& current);
        void UnregisterDockStatusChanged(CleanTankFlow& flow);

        static constexpr Dreame::DockMonitor::ListenerId NoListener = std::numeric_limits<Dreame::DockMonitor::ListenerId>::max();

        // Set by the dock monitor thread, consumed by Run.
        Dreame::DockMonitor::ListenerId DockStatusChangedEventId = NoListener;
    };
}