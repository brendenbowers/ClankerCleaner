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

#include "GPIO/Pin.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::GPIO
{
    class PinMonitor
    {
    public:
        bool Start();
        bool Stop() { return monitorThread.request_stop(); }
        bool StopAndWait();
        
        void MonitorPin(const std::uint32_t pin, const bool risingIsTrigger = true);
        void MonitorPin(Pin pin);

        Threading::ManualResetEvent& GetSync() { return sync; }

        std::vector<PinState> GetSince(std::uint64_t changedNS);


    private:
        void MonitorThread(std::stop_token stopToken);

        std::map<std::uint32_t,Pin> monitoredPins;

        std::map<std::uint32_t,PinState> states;

        std::jthread monitorThread;
        std::mutex updateMutex;

        Threading::ManualResetEvent sync;
    };
}