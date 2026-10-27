#pragma once

#include <chrono>
#include "StateMachine/State.h"
#include "Flows/DirtyTank/DirtyTank.h"


namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class EmptyTank : public StateWithFlow<DirtyTankFlow>
    {
    public:
        EmptyTank();
        ~EmptyTank() override = default;

        Transition Begin(DirtyTankFlow& flow) override;
        Transition Run(DirtyTankFlow& flow) override;
        bool End(DirtyTankFlow& flow) override;
        void Abort(DirtyTankFlow& flow) override;
    private:

        bool StopPumping(DirtyTankFlow& flow);
        std::chrono::steady_clock::time_point pumpStart;
    };
}