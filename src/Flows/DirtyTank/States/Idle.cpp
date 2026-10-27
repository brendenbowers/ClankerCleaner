#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"


namespace ClankerCleaner::Flows::DirtyTank::States
{
    Idle::Idle()
        : StateWithFlow<DirtyTankFlow>(Constants::State::IDLE_STATE)
        , lastEvent(0)
    {
    }

    Transition Idle::Begin(DirtyTankFlow& flow)
    {
        mopWashFinished = false;
        if(!flow.settings || !flow.settings->Dreame.Enabled || !flow.settings->Dreame.Push || !flow.dockMonitor)
        {
            return Transition::Next();
        }

        DockStatusChangedEventId = flow.dockMonitor->AddChangeListener(std::bind(&Idle::HandleDockStatusChanged, this, std::placeholders::_1, std::placeholders::_2));


        return Transition::Next();
    }

    Transition Idle::Run(DirtyTankFlow& flow)
    {
        if(mopWashFinished.exchange(false))
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

    bool Idle::End(DirtyTankFlow& flow)
    {
        UnregisterDockStatusChanged(flow);
        return true;
    }

    void Idle::Abort(DirtyTankFlow& flow)
    {
        UnregisterDockStatusChanged(flow);
    }

    // Runs on a dock monitor thread. Latching the result means a finished wash isn't lost
    // if another change arrives before the next Run.
    void Idle::HandleDockStatusChanged(const Dreame::DockStatus& previous, const Dreame::DockStatus& current)
    {
        const bool interrupted = current.Robot == Dreame::RobotState::WashingPaused ||
                                 current.Robot == Dreame::RobotState::Paused ||
                                 current.Robot == Dreame::RobotState::Error;
        if(previous.IsWashingMop() && !current.IsWashingMop() && !interrupted)
        {
            mopWashFinished = true;
        }
    }

    void Idle::UnregisterDockStatusChanged(DirtyTankFlow& flow)
    {
        if(flow.dockMonitor && DockStatusChangedEventId != NoListener)
        {
            // Once this returns the listener can't be running, so `this` is safe to drop.
            flow.dockMonitor->RemoveChangeListener(DockStatusChangedEventId);
        }
        DockStatusChangedEventId = NoListener;
        mopWashFinished = false;
    }

}