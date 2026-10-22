#pragma once

#include <cstdint>

// Platform-neutral pin types. Nothing here may include a platform GPIO library: the
// platform code lives in <Name>.<Platform>.cpp files, and CMake compiles only the target's.
namespace ClankerCleaner::GPIO
{
    // Collects line settings before the lines are claimed. Each platform defines it in its
    // own Platform.<Platform>.h, which only that platform's .cpp files include.
    struct PlatformLineConfig;
    enum class Edge
    {
        Rising,
        Falling
    };

    enum class Bias
    {
        None,
        PullUp,
        PullDown
    };

    // Latest edge seen on an input pin.
    struct PinState
    {
        std::uint64_t ChangedNS;
        std::uint32_t Sequence;
        std::uint16_t PinId;
        bool Triggered;
    };

    // A GPIO line, identified by the platform's line number (the BCM GPIO number on a Pi).
    class Pin
    {
    public:
        explicit Pin(std::uint32_t pinId);
        virtual ~Pin() = default;

        std::uint32_t GetPinId() const { return pinId; }

        // Adds this pin's line settings. Defined per platform, e.g. InputPin.RPi.cpp.
        virtual void Configure(PlatformLineConfig& config) const = 0;

    protected:
        Pin(const Pin&) = default;
        Pin& operator=(const Pin&) = default;

        std::uint32_t pinId = 0;
    };
}
