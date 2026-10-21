#include "GPIO/PinMonitor.h"
#include "GPIO/Constants.h"
#include <mutex>

namespace ClankerCleaner::GPIO
{

    bool PinMonitor::Start()
    {
        if(monitorThread.joinable())
        {
            return false;
        }

        // jthread only passes its stop token to a callable that takes it as the first argument.
        monitorThread = std::jthread([this](std::stop_token stopToken) { MonitorThread(stopToken); });
        return true;
    }

    bool PinMonitor::StopAndWait()
    {
        if(Stop())
        {
            monitorThread.join();
            return true;
        }

        return false;
    }

    void PinMonitor::MonitorPin(std::uint32_t pin, const bool risingIsTrigger)
    {
        Pin toMonitor(pin);
        toMonitor.SetTriggeredType(risingIsTrigger ? gpiod::edge_event::event_type::RISING_EDGE : gpiod::edge_event::event_type::FALLING_EDGE);
        MonitorPin(toMonitor);
    }

    void PinMonitor::MonitorPin(Pin pin)
    {
        if(monitorThread.joinable())
        {
            return;
        }

        monitoredPins.emplace(pin.GetPinId(), std::move(pin));
    }

    std::vector<PinState> PinMonitor::GetSince(std::uint64_t changedNS)
    {
        std::vector<PinState> updated;

        {
            std::unique_lock<std::mutex>lock(updateMutex);
            for(auto& entry : monitoredPins)
            {
                PinState state = entry.second.GetState();
                if(state.ChangedNS >= changedNS)
                {
                    updated.emplace_back(std::move(state));
                }
            }
        }

        return updated;
    }

    void PinMonitor::MonitorThread(std::stop_token stopToken)
    {
        gpiod::chip gpioChip(Constants::CHIP);
        gpiod::line_config config = gpiod::line_config();

        for(auto& pinEntry : monitoredPins)
        {
            pinEntry.second.Configure(config);
        }

        gpiod::line_request request = gpioChip.prepare_request()
            .set_consumer(Constants::CONSUMER)
            .set_line_config(config)
            .do_request();

        gpiod::edge_event_buffer eventBuffer(32);

        while(!stopToken.stop_requested())
        {
            if(request.wait_edge_events(std::chrono::seconds(1)))
            {
                request.read_edge_events(eventBuffer);

                {
                    std::unique_lock<std::mutex> lock(updateMutex);
                    for(const gpiod::edge_event& event : eventBuffer)
                    {
                        auto pinEntry = monitoredPins.find(event.line_offset());
                        if(pinEntry != monitoredPins.end())
                        {
                            pinEntry->second.HandleEvent(event);
                        }
                    }
                }

                sync.Set();
            }
        }
    }
}