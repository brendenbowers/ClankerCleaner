#pragma once

#include <chrono>
#include "StateMachine/State.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"


namespace ClankerCleaner::Flows::DirtyTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class EmptyTank : public StateWithFlow<DirtyTankFlow>
    {
    public:
        // runTime is how long the pump runs each time this state is entered, plus
        // fullTankExtraTime when the tank sensor reads wet (full) as the pump starts. While the
        // mops are being washed it runs on past that (see DirtyTankSettings::MopWashWetWindow).
        EmptyTank(const std::string name, std::chrono::seconds runTime,
                  std::chrono::seconds fullTankExtraTime = std::chrono::seconds(0));
        ~EmptyTank() override = default;

        Transition Begin(DirtyTankFlow& flow) override;
        Transition Run(DirtyTankFlow& flow) override;
        bool End(DirtyTankFlow& flow) override;
        void Abort(DirtyTankFlow& flow) override;
    private:

        bool StopPumping(DirtyTankFlow& flow);
        std::chrono::steady_clock::time_point pumpStart;
        std::chrono::seconds runTime;
        std::chrono::seconds fullTankExtraTime;
        // This run's time, chosen in Begin.
        std::chrono::seconds currentRunTime;
        // So pumping on through a mop wash is logged once per run, not every tick.
        bool keepingUpLogged = false;
    };
}