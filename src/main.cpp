#include <gpiod.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstdint>

#include "GPIO/PinMonitor.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"

namespace
{
    // Global atomic flag to safely communicate with the signal handler
    std::atomic<bool> run{true};

    // Signal handlers must use C linkage
    extern "C" void signal_handler(int signal) {
        if (signal == SIGINT) {
            run = false; // Safe to do inside a signal handler
        }
    }
}


int main()
{
    std::signal(SIGINT, signal_handler);
    
    ClankerCleaner::GPIO::PinMonitor monitor;
    monitor.MonitorPin(ClankerCleaner::GPIO::Constants::HIGH_WASTE_WATER_TANK);
    monitor.Start();

    std::uint64_t lastEvent = 0;
    while (run)
    {
        auto& sync = monitor.GetSync();
        if(sync.WaitOne(1000))
        {
            sync.Reset();
            for(const ClankerCleaner::GPIO::PinState& state : monitor.GetSince(lastEvent))
            {
                if(lastEvent < state.ChangedNS)
                {
                    lastEvent = state.ChangedNS;
                }
                const char* waterDetection = state.Triggered ? "NO WATER" : "WATER";
                std::cout << "Detected " << waterDetection << std::endl;
            }
        }
    }
    

    monitor.StopAndWait();

    return EXIT_SUCCESS;
}
