#pragma once

#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/DirtyTank/DirtyTank.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class Rinse : public StateWithFlow<Flows::DirtyTank::DirtyTankFlow>
    {
    public:
        Rinse();
        ~Rinse() override = default;

        Transition Begin(Flows::DirtyTank::DirtyTankFlow& flow) override;
        Transition Run(Flows::DirtyTank::DirtyTankFlow& flow) override;
        bool End(Flows::DirtyTank::DirtyTankFlow& flow) override;
        void Abort(Flows::DirtyTank::DirtyTankFlow& flow) override;

    private:
        bool StopRinsing(Flows::DirtyTank::DirtyTankFlow& flow);

        std::chrono::steady_clock::time_point rinseStart;
    };
}