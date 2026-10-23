#pragma once

#include "StateMachine/Flow.h"
#include "StateMachine/Machine.h"
#include "GPIO/PinInterface.h"
#include "Config/Config.h"


namespace ClankerCleaner::Flows::DirtyTank
{
    class DirtyTankFlow : public StateMachine::Flow
    {
    public:
        DirtyTankFlow(StateMachine::Machine& machine, std::string name);

        GPIO::PinInterface* pinMonitor;
        const Config::Settings* settings = nullptr;
    };
}