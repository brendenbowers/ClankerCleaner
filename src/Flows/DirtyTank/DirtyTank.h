#pragma once

#include "StateMachine/Flow.h"
#include "StateMachine/Machine.h"
#include "GPIO/PinInterface.h"
#include "Config/Config.h"
#include "Dreame/DockMonitor.h"


namespace ClankerCleaner::Flows::DirtyTank
{
    constexpr const char* FLOWNAME = "DirtyTankFlow";

    class DirtyTankFlow : public StateMachine::Flow
    {
    public:
        DirtyTankFlow(StateMachine::Machine& machine, std::string name);

        GPIO::PinInterface* pinMonitor;
        const Config::Settings* settings = nullptr;
        // Robot and dock state from the cloud; nullptr when [dreame] is disabled.
        const Dreame::DockMonitor* dockMonitor = nullptr;
    };
}