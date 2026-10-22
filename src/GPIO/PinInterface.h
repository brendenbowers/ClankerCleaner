#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include "GPIO/InputPin.h"
#include "GPIO/OutputPin.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::GPIO
{
    // Platform-neutral GPIO access: inputs report edges, outputs are driven from any thread
    // through SetOutput. Add pins before Start().
    //
    // Shared logic is in PinInterface.cpp. The constructor, destructor and the Platform*
    // functions are defined in PinInterface.<Platform>.cpp, along with PlatformState.
    class PinInterface
    {
    public:
        PinInterface();
        ~PinInterface();

        PinInterface(const PinInterface&) = delete;
        PinInterface& operator=(const PinInterface&) = delete;

        // Claims the lines; outputs start inactive (off).
        bool Start();
        // Turns every output off, stops watching inputs and releases the lines.
        bool StopAndWait();
        bool IsStarted() const { return started; }

        void MonitorPin(const std::uint32_t pin, const Edge triggerEdge = Edge::Falling);
        void MonitorPin(InputPin pin);

        void AddOutputPin(const std::uint32_t pin, const bool activeLow = false);
        void AddOutputPin(OutputPin pin);

        // Safe from any thread. False if the pin isn't a known output or the interface isn't started.
        bool SetOutput(std::uint32_t pin, bool active);
        bool IsOutputActive(std::uint32_t pin) const;
        void AllOutputsOff();

        // Set whenever an input edge arrives.
        Threading::ManualResetEvent& GetSync() { return sync; }

        std::vector<PinState> GetSince(std::uint64_t changedNS);
        PinState GetState(std::uint32_t pinId);

    private:
        // Platform hooks, defined in PinInterface.<Platform>.cpp.
        struct PlatformState;
        void PlatformStart();
        void PlatformStop();
        void PlatformSetOutput(std::uint32_t pin, bool active);

        // Called by the platform code when an input changes.
        void OnEdge(std::uint32_t pinId, Edge edge, std::uint64_t timestampNs, std::uint32_t sequence);

        std::unique_ptr<PlatformState> platformState;
        bool started = false;

        std::mutex updateMutex;
        std::map<std::uint32_t,InputPin> monitoredPins;

        mutable std::mutex outputMutex;
        std::map<std::uint32_t,OutputPin> outputPins;

        Threading::ManualResetEvent sync;
    };
}
