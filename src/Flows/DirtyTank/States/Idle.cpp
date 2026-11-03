#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"

#include <algorithm>
#include <vector>

namespace ClankerCleaner::Flows::DirtyTank::States
{
    Idle::Idle()
        : StateWithFlow<DirtyTankFlow>(Constants::State::IDLE_STATE)
        , lastEvent(0)
    {
    }

    Transition Idle::Begin(DirtyTankFlow& flow)
    {
        // A wash that finished while the tank was being emptied is already dealt with.
        flow.dock.ClearWashFinished();
        return Transition::Next();
    }

    Transition Idle::Run(DirtyTankFlow& flow)
    {
        if(flow.dock.TakeWashFinished())
        {
            return Transition::Next();
        }


        // todo: maybe an optimization checking first if there are changes
        // GetSince includes events at exactly lastEvent, so +1 skips the one already handled.
        std::vector<GPIO::PinState> changedPins = flow.pinMonitor->GetSince(lastEvent + 1);

        const std::uint32_t sensorPin = flow.settings->Gpio.DirtyTankSensorPin;
        auto it = std::find_if(changedPins.begin(), changedPins.end(),
            [sensorPin](GPIO::PinState pinState) { return pinState.PinId == sensorPin; });
        if(it == changedPins.end())
        {
            return Transition::Stay();
        }

        std::uint64_t debounceTime = (std::chrono::steady_clock::now() -  flow.settings->DirtyTank.SensorDebounce)
            .time_since_epoch()
            .count();
        if(it->ChangedNS > debounceTime)
        {
            // delay a bit to avoid false triggers from the sensor bouncing
            return Transition::Stay();
        }

        GPIO::PinState& pinState = *it;
        lastEvent = pinState.ChangedNS;

        if(pinState.Triggered)
        {
            return Transition::Next();
        }

        return Transition::Stay();
    }

    bool Idle::End(DirtyTankFlow& /*flow*/)
    {
        return true;
    }

    void Idle::Abort(DirtyTankFlow& /*flow*/)
    {
    }
}
