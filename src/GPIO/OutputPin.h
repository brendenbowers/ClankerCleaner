#pragma once
#include <gpiod.hpp>

#include <cstdint>

#include "GPIO/Pin.h"

namespace ClankerCleaner::GPIO
{
    // Output that always starts inactive (off). activeLow flips the polarity, e.g. for a
    // relay board with its trigger jumper on L, so "active" always means "on".
    class OutputPin : public Pin
    {
    public:
        explicit OutputPin(std::uint32_t pinId, bool activeLow = false);

        OutputPin(const OutputPin&) = default;
        OutputPin& operator=(const OutputPin&) = default;

        void Configure(gpiod::line_config& config) const override;

        bool IsActiveLow() const { return activeLow; }

        // Last value set through PinInterface.
        bool IsActive() const { return active; }

    private:
        friend class PinInterface;

        bool activeLow;
        bool active = false;
    };
}
