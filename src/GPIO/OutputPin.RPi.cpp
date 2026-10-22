#include "GPIO/OutputPin.h"
#include "GPIO/Platform.RPi.h"

namespace ClankerCleaner::GPIO
{
    void OutputPin::Configure(PlatformLineConfig& config) const
    {
        config.Lines.add_line_settings(pinId, gpiod::line_settings()
            .set_direction(gpiod::line::direction::OUTPUT)
            .set_active_low(activeLow)
            .set_output_value(gpiod::line::value::INACTIVE));
    }
}
