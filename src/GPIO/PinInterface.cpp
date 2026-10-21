#include "GPIO/PinInterface.h"
#include "GPIO/Constants.h"
#include <mutex>

namespace ClankerCleaner::GPIO
{
    PinInterface::~PinInterface()
    {
        StopAndWait();
    }

    bool PinInterface::Start()
    {
        if(started)
        {
            return false;
        }

        // Outputs get their own request so SetOutput never touches the monitor thread's request.
        if(!outputPins.empty())
        {
            gpiod::chip gpioChip(Constants::CHIP);
            gpiod::line_config config = gpiod::line_config();
            for(const auto& pinEntry : outputPins)
            {
                pinEntry.second.Configure(config);
            }

            std::lock_guard<std::mutex> lock(outputMutex);
            outputRequest.emplace(gpioChip.prepare_request()
                .set_consumer(Constants::CONSUMER)
                .set_line_config(config)
                .do_request());
            for(auto& pinEntry : outputPins)
            {
                pinEntry.second.active = false;
            }
        }

        // Requesting zero input lines would throw, so only start the thread when there are inputs.
        if(!monitoredPins.empty())
        {
            // jthread only passes its stop token to a callable that takes it as the first argument.
            monitorThread = std::jthread([this](std::stop_token stopToken) { MonitorThread(stopToken); });
        }

        started = true;
        return true;
    }

    bool PinInterface::StopAndWait()
    {
        if(!started)
        {
            return false;
        }

        if(monitorThread.joinable())
        {
            monitorThread.request_stop();
            monitorThread.join();
        }

        AllOutputsOff();
        {
            std::lock_guard<std::mutex> lock(outputMutex);
            outputRequest.reset();
        }

        started = false;
        return true;
    }

    void PinInterface::MonitorPin(std::uint32_t pin, const bool risingIsTrigger)
    {
        MonitorPin(InputPin(pin, risingIsTrigger ? gpiod::edge_event::event_type::RISING_EDGE : gpiod::edge_event::event_type::FALLING_EDGE));
    }

    void PinInterface::MonitorPin(InputPin pin)
    {
        if(started)
        {
            return;
        }

        monitoredPins.emplace(pin.GetPinId(), std::move(pin));
    }

    void PinInterface::AddOutputPin(std::uint32_t pin, const bool activeLow)
    {
        AddOutputPin(OutputPin(pin, activeLow));
    }

    void PinInterface::AddOutputPin(OutputPin pin)
    {
        if(started)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(outputMutex);
        outputPins.emplace(pin.GetPinId(), std::move(pin));
    }

    bool PinInterface::SetOutput(std::uint32_t pin, bool active)
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        auto pinEntry = outputPins.find(pin);
        if(pinEntry == outputPins.end() || !outputRequest)
        {
            return false;
        }

        outputRequest->set_value(pin, active ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE);
        pinEntry->second.active = active;
        return true;
    }

    bool PinInterface::IsOutputActive(std::uint32_t pin) const
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        auto pinEntry = outputPins.find(pin);
        return pinEntry != outputPins.end() && pinEntry->second.IsActive();
    }

    void PinInterface::AllOutputsOff()
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        if(!outputRequest)
        {
            return;
        }

        for(auto& pinEntry : outputPins)
        {
            outputRequest->set_value(pinEntry.first, gpiod::line::value::INACTIVE);
            pinEntry.second.active = false;
        }
    }

    std::vector<PinState> PinInterface::GetSince(std::uint64_t changedNS)
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

    void PinInterface::MonitorThread(std::stop_token stopToken)
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
