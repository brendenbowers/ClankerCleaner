#pragma once

#include "Config/Config.h"
#include "GPIO/PinInterface.h"

namespace ClankerCleaner::Setup
{
    // Registers the monitored and output pins from the settings and starts the pin interface.
    // Each level sensor's trigger edge and bias follow its SensorOutput setting.
    void ConfigurePins(GPIO::PinInterface& pins, Config::Settings& settings);
}
