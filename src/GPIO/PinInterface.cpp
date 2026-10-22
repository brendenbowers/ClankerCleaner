// Platform-neutral half of PinInterface; the rest is in PinInterface.<Platform>.cpp.

#include "GPIO/PinInterface.h"

namespace ClankerCleaner::GPIO
{
    bool PinInterface::Start()
    {
        if(started)
        {
            return false;
        }

        {
            std::lock_guard<std::mutex> lock(outputMutex);
            for(auto& pinEntry : outputPins)
            {
                pinEntry.second.active = false;
            }
        }

        PlatformStart();
        started = true;
        return true;
    }

    bool PinInterface::StopAndWait()
    {
        if(!started)
        {
            return false;
        }

        AllOutputsOff();
        PlatformStop();
        started = false;
        return true;
    }

    void PinInterface::MonitorPin(std::uint32_t pin, const Edge triggerEdge)
    {
        MonitorPin(InputPin(pin, triggerEdge));
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
        if(pinEntry == outputPins.end() || !started)
        {
            return false;
        }

        PlatformSetOutput(pin, active);
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
        if(!started)
        {
            return;
        }

        for(auto& pinEntry : outputPins)
        {
            PlatformSetOutput(pinEntry.first, false);
            pinEntry.second.active = false;
        }
    }

    std::vector<PinState> PinInterface::GetSince(std::uint64_t changedNS)
    {
        std::vector<PinState> updated;

        std::lock_guard<std::mutex> lock(updateMutex);
        for(const auto& entry : monitoredPins)
        {
            PinState state = entry.second.GetState();
            if(state.ChangedNS >= changedNS)
            {
                updated.push_back(state);
            }
        }

        return updated;
    }

    PinState PinInterface::GetState(std::uint32_t pinId)
    {
        std::lock_guard<std::mutex> lock(updateMutex);
        auto pinEntry = monitoredPins.find(pinId);
        return pinEntry != monitoredPins.end() ? pinEntry->second.GetState() : PinState{};
    }

    void PinInterface::OnEdge(std::uint32_t pinId, Edge edge, std::uint64_t timestampNs, std::uint32_t sequence)
    {
        {
            std::lock_guard<std::mutex> lock(updateMutex);
            auto pinEntry = monitoredPins.find(pinId);
            if(pinEntry == monitoredPins.end())
            {
                return;
            }
            pinEntry->second.HandleEdge(edge, timestampNs, sequence);
        }

        sync.Set();
    }
}
