#include "GPIO/OutputPin.h"

namespace ClankerCleaner::GPIO
{
    OutputPin::OutputPin(std::uint32_t pinId, bool activeLow)
        : Pin(pinId)
        , activeLow(activeLow)
    {
    }

    void OutputPin::Configure(gpiod::line_config& config) const
    {
        const gpiod::line_settings pinSettings = gpiod::line_settings()
            .set_direction(gpiod::line::direction::OUTPUT)
            .set_active_low(activeLow)
            .set_output_value(gpiod::line::value::INACTIVE);
        config.add_line_settings(pinId, pinSettings);
    }
}
