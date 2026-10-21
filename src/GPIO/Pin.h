#pragma once
#include <gpiod.hpp>

#include <atomic>
#include <mutex>
#include <cstdlib>


namespace ClankerCleaner::GPIO
{

    struct PinState
    {
        std::uint64_t ChangedNS;
        std::uint32_t Sequence;
        bool Triggered;
    };

    class Pin 
    {
    public:
        Pin(std::uint64_t pinId);
        Pin(const Pin& other);

        std::uint32_t GetPinId() { return pinId; }

        void Configure(gpiod::line_config& config);
        void HandleEvent(const gpiod::edge_event& event);

        PinState GetState();

        void SetTriggeredType(gpiod::edge_event::event_type type) { triggeredType = type; }

    private:
        std::uint32_t pinId = 0;
        gpiod::edge_event::event_type triggeredType = gpiod::edge_event::event_type::FALLING_EDGE;
        //std::mutex stateMutex;
        PinState state{};        
    };
}