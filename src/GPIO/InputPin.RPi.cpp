#include "GPIO/InputPin.h"
#include "GPIO/Platform.RPi.h"

namespace ClankerCleaner::GPIO
{
    namespace
    {
        gpiod::line::bias ToGpiodBias(Bias bias)
        {
            switch(bias)
            {
            case Bias::PullUp: return gpiod::line::bias::PULL_UP;
            case Bias::PullDown: return gpiod::line::bias::PULL_DOWN;
            case Bias::None: break;
            }
            return gpiod::line::bias::DISABLED;
        }
    }

    void InputPin::Configure(PlatformLineConfig& config) const
    {
        config.Lines.add_line_settings(pinId, gpiod::line_settings()
            .set_direction(gpiod::line::direction::INPUT)
            .set_edge_detection(gpiod::line::edge::BOTH)
            .set_bias(ToGpiodBias(bias)));
    }
}
