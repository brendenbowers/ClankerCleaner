#pragma once
#include <gpiod.hpp>

#include <cstdint>

#include "GPIO/Pin.h"

namespace ClankerCleaner::GPIO
{
    // Input with the pull-up on and edge events on both edges. Tracks the latest edge as a PinState.
    class InputPin : public Pin
    {
    public:
        explicit InputPin(std::uint32_t pinId,
                          gpiod::edge_event::event_type triggeredType = gpiod::edge_event::event_type::FALLING_EDGE);

        InputPin(const InputPin&) = default;
        InputPin& operator=(const InputPin&) = default;

        void Configure(gpiod::line_config& config) const override;

        // Edge that counts as "triggered" in the PinState.
        void SetTriggeredType(gpiod::edge_event::event_type type) { triggeredType = type; }

        void HandleEvent(const gpiod::edge_event& event);
        PinState GetState() const { return state; }

    private:
        gpiod::edge_event::event_type triggeredType;
        PinState state{};
    };
}
