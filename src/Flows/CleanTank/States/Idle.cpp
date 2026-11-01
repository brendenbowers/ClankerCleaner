#include "Flows/CleanTank/States/Idle.h"
#include "Flows/CleanTank/CleanTank.h"
#include "Flows/CleanTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"
#include "Logging/Logging.h"


namespace ClankerCleaner::Flows::CleanTank::States
{
    Idle::Idle()
        : StateWithFlow<CleanTankFlow>(Constants::State::IDLE_STATE)
    {
    }

    Transition Idle::Begin(CleanTankFlow& flow)
    {
        Logging::Get(flow.GetName())->debug("High Pin: {}. Low Pin: {}", flow.settings->Gpio.CleanTankHighSensorPin, flow.settings->Gpio.CleanTankLowSensorPin);

        if(!flow.settings || !flow.settings->Dreame.Enabled || !flow.settings->Dreame.Push || !flow.dockMonitor)
        {
            return Transition::Next();
        }

        DockStatusChangedEventId = flow.dockMonitor->AddChangeListener(std::bind(&Idle::HandleDockStatusChanged, this, std::placeholders::_1, std::placeholders::_2));

        return Transition::Next();
    }

    Transition Idle::Run(CleanTankFlow& flow)
    {
        using seconds = std::chrono::seconds;
        using ns = std::chrono::nanoseconds;
        using time_point = std::chrono::steady_clock::time_point;

        GPIO::PinState hightPinState = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankHighSensorPin);
        if(hightPinState == GPIO::PinState::Default)
        {
            Logging::Get(flow.GetName())->warn("unable to get pin state for high clean water tank sensor");
            return Transition::Stay();
        }
        
        if(hightPinState.Triggered && 
            time_point(ns(hightPinState.ChangedNS)) + seconds(flow.settings->CleanTank.SensorDebounce) > std::chrono::steady_clock::now())
        {
            return Transition::Stay();
        }


        GPIO::PinState lowPinState = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankLowSensorPin);
        if(lowPinState == GPIO::PinState::Default)
        {
            Logging::Get(flow.GetName())->warn("unable to get pin state for low clean water tank sensor");
            return Transition::Stay();
        }

        if(lowPinState.Triggered && 
            time_point(ns(lowPinState.ChangedNS)) + seconds(flow.settings->CleanTank.SensorDebounce) > std::chrono::steady_clock::now())
        {
            return Transition::Stay();
        }

        return Transition::Next();
    }

    bool Idle::End(CleanTankFlow& flow)
    {
        UnregisterDockStatusChanged(flow);
        return true;
    }

    void Idle::Abort(CleanTankFlow& flow)
    {
        UnregisterDockStatusChanged(flow);
    }

    // Runs on a dock monitor thread. Latching the result means a finished wash isn't lost
    // if another change arrives before the next Run.
    void Idle::HandleDockStatusChanged(const Dreame::DockStatus& /*previous*/, const Dreame::DockStatus& /*current*/)
    {
        // todo: do checking if the dock used water
    }

    void Idle::UnregisterDockStatusChanged(CleanTankFlow& flow)
    {
        if(flow.dockMonitor && DockStatusChangedEventId != NoListener)
        {
            // Once this returns the listener can't be running, so `this` is safe to drop.
            flow.dockMonitor->RemoveChangeListener(DockStatusChangedEventId);
        }
        DockStatusChangedEventId = NoListener;
    }
}