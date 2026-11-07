#pragma once

#include "StateMachine/Flow.h"
#include "StateMachine/Machine.h"
#include "GPIO/PinInterface.h"
#include "Config/Config.h"
#include "Flows/CleanTank/CleanTank.h"
#include "Flows/DirtyTank/DirtyTank.h"

namespace ClankerCleaner::Flows::Commands
{
    // Takes commands from MQTT. It drives no outputs itself: a command moves a waiting tank flow
    // to one of its own states, which does the work with its usual checks, so each output still
    // has one owner. That one step runs, then the flow goes back to Idle (see ReturnToIdle). A
    // tank flow that's already busy refuses the command.
    // Only stop interrupts a tank flow; it stays stopped until resume or a restart.
    class CommandFlow : public StateMachine::Flow
    {
    public:
        CommandFlow(StateMachine::Machine& machine, std::string name);

        GPIO::PinInterface* pinMonitor = nullptr;
        const Config::Settings* settings = nullptr;
        DirtyTank::DirtyTankFlow* dirtyTank = nullptr;
        CleanTank::CleanTankFlow* cleanTank = nullptr;
    };
}
