#pragma once

#include "StateMachine/Flow.h"
#include "StateMachine/Machine.h"
#include "GPIO/PinInterface.h"
#include "Config/Config.h"
#include "Dreame/DockMonitor.h"
#include "Flows/DockWatch.h"


namespace ClankerCleaner::Flows::CleanTank 
{
    class CleanTankFlow : public StateMachine::Flow
    {
    public:
        CleanTankFlow(StateMachine::Machine& machine, std::string name);

        GPIO::PinInterface* pinMonitor;
        const Config::Settings* settings = nullptr;
        // Robot and dock state from the cloud; nullptr when [dreame] is disabled.
        Dreame::DockMonitor* dockMonitor = nullptr;
        // Whether the mops are being washed; never watching (always false) without push updates.
        DockWatch dock;
    };
}