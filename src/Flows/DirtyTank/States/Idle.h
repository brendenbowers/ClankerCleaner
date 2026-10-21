#pragma once

#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class Idle : public State
    {
    public:
        Idle();
        ~Idle() override = default;

        Transition Run(Flow& flow) override;
    private:
        std::uint64_t lastEvent;
    };
}