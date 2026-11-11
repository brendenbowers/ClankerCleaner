#pragma once

#include <chrono>
#include <string>

#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/DirtyTank/DirtyTank.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief opens the rinse solenoid for a set time, or until the dirty tank sensor reads wet.
    /// Used both to rinse the tank and, after the final empty, to put a little water back in.
    class Rinse : public StateWithFlow<Flows::DirtyTank::DirtyTankFlow>
    {
    public:
        Rinse(const std::string name, std::chrono::seconds duration);
        ~Rinse() override = default;

        Transition Begin(Flows::DirtyTank::DirtyTankFlow& flow) override;
        Transition Run(Flows::DirtyTank::DirtyTankFlow& flow) override;
        bool End(Flows::DirtyTank::DirtyTankFlow& flow) override;
        void Abort(Flows::DirtyTank::DirtyTankFlow& flow) override;

    private:
        bool StopRinsing(Flows::DirtyTank::DirtyTankFlow& flow);

        std::chrono::seconds duration;
        std::chrono::steady_clock::time_point rinseStart;
    };
}