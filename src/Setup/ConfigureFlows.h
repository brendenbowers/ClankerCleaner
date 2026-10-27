#pragma once

#include "Config/Config.h"
#include "Dreame/DockMonitor.h"
#include "GPIO/PinInterface.h"
#include "StateMachine/Machine.h"

namespace ClankerCleaner::Setup
{
    // Adds the flows and their states to the machine and wires in their dependencies.
    // Idle waits for the sensor, EmptyTank runs the pump, then back to Idle.
    void ConfigureFlows(StateMachine::Machine& machine, GPIO::PinInterface& pins, Config::Settings& settings,
                        Dreame::DockMonitor* dockMonitor);
}
