#pragma once

#include "Config/Config.h"
#include "GPIO/PinInterface.h"

namespace ClankerCleaner::Setup
{
    // Registers the monitored and output pins from the settings and starts the pin interface.
    // The dirty tank sensor pulls LOW when wet, so a falling edge is the trigger.
    void ConfigurePins(GPIO::PinInterface& pins, Config::Settings& settings);
}
