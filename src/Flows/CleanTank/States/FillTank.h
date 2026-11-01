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
    class FillTank : public StateWithFlow<CleanTankFlow>
    {
    public:
        FillTank();
        ~FillTank() override = default;

        Transition Begin(CleanTankFlow& flow) override;
        Transition Run(CleanTankFlow& flow) override;
        bool End(CleanTankFlow& flow) override;
        void Abort(CleanTankFlow& flow) override;
    private:
        bool StopFillingTank(CleanTankFlow& flow);

        std::chrono::steady_clock::time_point fillStart;
    };
}