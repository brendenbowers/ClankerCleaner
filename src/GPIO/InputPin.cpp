#include "GPIO/InputPin.h"

namespace ClankerCleaner::GPIO
{
    InputPin::InputPin(std::uint32_t pinId, Edge triggerEdge, Bias bias)
        : Pin(pinId)
        , triggerEdge(triggerEdge)
        , bias(bias)
    {
        state.PinId = static_cast<std::uint16_t>(pinId);
    }

    void InputPin::HandleEdge(Edge edge, std::uint64_t timestampNs, std::uint32_t sequence)
    {
        state.PinId = static_cast<std::uint16_t>(pinId);
        state.Triggered = edge == triggerEdge;
        state.ChangedNS = timestampNs;
        state.Sequence = sequence;
    }
}
