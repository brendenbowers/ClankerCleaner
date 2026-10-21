#pragma once
#include <gpiod.hpp>

#include <cstdint>

namespace ClankerCleaner::GPIO
{
    // Latest edge seen on an input pin.
    struct PinState
    {
        std::uint64_t ChangedNS;
        std::uint32_t Sequence;
        std::uint16_t PinId;
        bool Triggered;
    };

    // A GPIO line on the chip. Subclasses decide how the line is configured (input, output).
    class Pin
    {
    public:
        explicit Pin(std::uint32_t pinId);
        virtual ~Pin() = default;

        std::uint32_t GetPinId() const { return pinId; }

        // Adds this pin's line settings to a request's config.
        virtual void Configure(gpiod::line_config& config) const = 0;

    protected:
        Pin(const Pin&) = default;
        Pin& operator=(const Pin&) = default;

        std::uint32_t pinId = 0;
    };
}
