#pragma once


#include <atomic>
#include <chrono>
#include <limits>
#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/CleanTank/CleanTank.h"

namespace ClankerCleaner::Flows::CleanTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class AddCleanerState : public StateWithFlow<CleanTankFlow>
    {
    public:
        AddCleanerState();
        ~AddCleanerState() override = default;

        Transition Begin(CleanTankFlow& flow) override;
        Transition Run(CleanTankFlow& flow) override;
        bool End(CleanTankFlow& flow) override;
        void Abort(CleanTankFlow& flow) override;
    private:
        bool StopAddingDetergent(CleanTankFlow& flow);

        std::chrono::steady_clock::time_point fillStart;
    };
}