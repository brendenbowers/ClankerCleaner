#pragma once

#include <chrono>
#include <cstdint>
#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/DirtyTank/DirtyTank.h"

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
        std::uint64_t lastEvent;
    };
}
