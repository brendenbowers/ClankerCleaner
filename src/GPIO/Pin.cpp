#include "GPIO/Pin.h"
#include <semaphore>

namespace ClankerCleaner::GPIO
{
    Pin::Pin(std::uint64_t pinId)
        : pinId(pinId)
    {
    }

    Pin::Pin(const Pin& other)
        : pinId(other.pinId)
        , triggeredType(other.triggeredType)
        , state(other.state)
    {
    }

    void Pin::Configure(gpiod::line_config &config)
    {
        const gpiod::line_settings pinSettings = gpiod::line_settings()
            .set_direction(gpiod::line::direction::INPUT)
            .set_edge_detection(gpiod::line::edge::BOTH)
            .set_bias(gpiod::line::bias::PULL_UP);
        config.add_line_settings(pinId, pinSettings);
    }

    void Pin::HandleEvent(const gpiod::edge_event &event)
    {
        if(event.line_offset() != pinId)
        {
            return;
        }
        
        state.Triggered = triggeredType == event.type();
        state.ChangedNS = event.timestamp_ns();
        state.Sequence = event.line_seqno();
    }

    PinState Pin::GetState()
    {
        return state;
    }


}
