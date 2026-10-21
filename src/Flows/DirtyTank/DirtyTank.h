#pragma once

#include "StateMachine/Flow.h"
#include "StateMachine/Machine.h"
#include "GPIO/PinInterface.h" 


namespace ClankerCleaner::Flows::DirtyTank
{
    class DirtyTankFlow : public StateMachine::Flow
    {
    public:
        DirtyTankFlow(StateMachine::Machine& machine, std::string name);

        GPIO::PinInterface* pinMonitor;  
    };
}