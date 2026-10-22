#pragma once

#include <cstdint>

#include "GPIO/Pin.h"

namespace ClankerCleaner::GPIO
{
    // Input watched for edges in both directions. Tracks the latest edge as a PinState,
    // with Triggered true when that edge matches the trigger edge.
    class InputPin : public Pin
    {
    public:
        explicit InputPin(std::uint32_t pinId, Edge triggerEdge = Edge::Falling, Bias bias = Bias::PullUp);

        InputPin(const InputPin&) = default;
        InputPin& operator=(const InputPin&) = default;

        // Defined in InputPin.<Platform>.cpp.
        void Configure(PlatformLineConfig& config) const override;

        Edge GetTriggerEdge() const { return triggerEdge; }
        void SetTriggerEdge(Edge edge) { triggerEdge = edge; }
        Bias GetBias() const { return bias; }

        // Called by PinInterface when the backend reports an edge on this pin.
        void HandleEdge(Edge edge, std::uint64_t timestampNs, std::uint32_t sequence);
        PinState GetState() const { return state; }

    private:
        Edge triggerEdge;
        Bias bias;
        PinState state{};
    };
}
