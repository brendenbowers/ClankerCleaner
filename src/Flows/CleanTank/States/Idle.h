#pragma once


#include <chrono>
#include "StateMachine/State.h"
#include "GPIO/PinInterface.h"
#include "Flows/CleanTank/CleanTank.h"


namespace ClankerCleaner::Flows::CleanTank::States
{

    using namespace ClankerCleaner::StateMachine;

    /// @brief the starting task to check the pins and transition to the clean state
    class Idle : public StateWithFlow<CleanTankFlow>
    {
    public:
        Idle();
        ~Idle() override = default;

        Transition Begin(CleanTankFlow& flow) override;
        Transition Run(CleanTankFlow& flow) override;
        bool End(CleanTankFlow& flow) override;
        void Abort(CleanTankFlow& flow) override;
    private:
        // So the sensor conflict is logged once per visit, not every tick.
        bool sensorConflictLogged = false;
    };
}