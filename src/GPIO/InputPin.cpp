#include "GPIO/InputPin.h"

namespace ClankerCleaner::GPIO
{
    InputPin::InputPin(std::uint32_t pinId, gpiod::edge_event::event_type triggeredType)
        : Pin(pinId)
        , triggeredType(triggeredType)
    {
        state.PinId = static_cast<std::uint16_t>(pinId);
    }

    void InputPin::Configure(gpiod::line_config& config) const
    {
        const gpiod::line_settings pinSettings = gpiod::line_settings()
            .set_direction(gpiod::line::direction::INPUT)
            .set_edge_detection(gpiod::line::edge::BOTH)
            .set_bias(gpiod::line::bias::PULL_UP);
        config.add_line_settings(pinId, pinSettings);
    }

    void InputPin::HandleEvent(const gpiod::edge_event& event)
    {
        if(event.line_offset() != pinId)
        {
            return;
        }

        state.PinId = event.line_offset();
        state.Triggered = triggeredType == event.type();
        state.ChangedNS = event.timestamp_ns();
        state.Sequence = event.line_seqno();
    }
}
