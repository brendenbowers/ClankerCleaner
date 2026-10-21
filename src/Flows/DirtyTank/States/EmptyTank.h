#pragma once

#include <chrono>
#include "StateMachine/State.h"
//#include "GPIO/PinInterface.h"


namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class EmptyTank : public State
    {
    public:
        EmptyTank();
        ~EmptyTank() override = default;

        Transition Begin(Flow& flow) override;
        Transition Run(Flow& flow) override;
        bool End(Flow& flow) override;
        void Abort(Flow& flow) override;
    private:

        bool StopPumping(Flow& flow);
        std::chrono::steady_clock::time_point pumpStart;
    };
}