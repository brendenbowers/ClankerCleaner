#pragma once

#include <chrono>
#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"


#include "Dreame/Types.h"
#include "Dreame/DockMonitor.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class Idle : public State
    {
    public:
        Idle();
        ~Idle() override = default;

        Transition Begin(Flow& flow) override;
        Transition Run(Flow& flow) override;
        bool End(Flow& flow) override;
        void Abort(Flow& flow) override;
    private:
        void HandleDockStatusChanged(const Dreame::DockStatus& prev, const Dreame::DockStatus& cur);
        void UnregisterDockStatusChanged(Flow& flow);


        Dreame::DockStatus Previous;
        Dreame::DockStatus Current;
        Dreame::DockMonitor::ListenerId DockStatusChangedEventId = std::numeric_limits<Dreame::DockMonitor::ListenerId>::max();
        std::uint64_t lastEvent;
    };
}