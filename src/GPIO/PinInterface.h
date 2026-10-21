#pragma once
#include <gpiod.hpp>

#include <atomic>
#include <mutex>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstdint>
#include <stop_token>
#include <unordered_set>

#include <map>
#include <optional>
#include <vector>

#include "GPIO/InputPin.h"
#include "GPIO/OutputPin.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::GPIO
{
    // Owns the GPIO line requests: inputs are watched on a monitor thread, outputs are
    // driven from any thread through SetOutput. Add pins before Start().
    class PinInterface
    {
    public:
        PinInterface() = default;
        ~PinInterface();

        PinInterface(const PinInterface&) = delete;
        PinInterface& operator=(const PinInterface&) = delete;

        // Requests the lines; outputs start inactive (off).
        bool Start();
        bool Stop() { return monitorThread.request_stop(); }
        // Stops the monitor thread, turns every output off and releases the lines.
        bool StopAndWait();
        bool IsStarted() const { return started; }

        void MonitorPin(const std::uint32_t pin, const bool risingIsTrigger = true);
        void MonitorPin(InputPin pin);

        void AddOutputPin(const std::uint32_t pin, const bool activeLow = false);
        void AddOutputPin(OutputPin pin);

        // Safe from any thread. False if the pin isn't a known output or the interface isn't started.
        bool SetOutput(std::uint32_t pin, bool active);
        bool IsOutputActive(std::uint32_t pin) const;
        void AllOutputsOff();

        Threading::ManualResetEvent& GetSync() { return sync; }

        std::vector<PinState> GetSince(std::uint64_t changedNS);


    private:
        void MonitorThread(std::stop_token stopToken);

        bool started = false;

        std::map<std::uint32_t,InputPin> monitoredPins;

        std::map<std::uint32_t,PinState> states;

        mutable std::mutex outputMutex;
        std::map<std::uint32_t,OutputPin> outputPins;
        std::optional<gpiod::line_request> outputRequest;

        std::jthread monitorThread;
        std::mutex updateMutex;

        Threading::ManualResetEvent sync;
    };
}
