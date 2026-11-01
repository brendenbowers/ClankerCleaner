#include "Flows/CleanTank/States/Idle.h"
#include "Flows/CleanTank/CleanTank.h"
#include "Flows/CleanTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"
#include "Logging/Logging.h"


namespace ClankerCleaner::Flows::CleanTank::States
{
    namespace
    {
        // True once the sensor has held its current reading for the debounce time.
        bool IsSettled(const GPIO::PinState& state, std::chrono::seconds debounce)
        {
            const std::chrono::steady_clock::time_point changed{std::chrono::nanoseconds(state.ChangedNS)};
            return changed + debounce <= std::chrono::steady_clock::now();
        }
    }

    Idle::Idle()
        : StateWithFlow<CleanTankFlow>(Constants::State::IDLE_STATE)
    {
    }

    Transition Idle::Begin(CleanTankFlow& flow)
    {
        sensorConflictLogged = false;
        if(!flow.settings)
        {
            return Transition::Next();
        }
        Logging::Get(flow.GetName())->debug("High Pin: {}. Low Pin: {}", flow.settings->Gpio.CleanTankHighSensorPin, flow.settings->Gpio.CleanTankLowSensorPin);

        if(!flow.settings->Dreame.Enabled || !flow.settings->Dreame.Push || !flow.dockMonitor)
        {
            return Transition::Next();
        }

        DockStatusChangedEventId = flow.dockMonitor->AddChangeListener(std::bind(&Idle::HandleDockStatusChanged, this, std::placeholders::_1, std::placeholders::_2));

        return Transition::Next();
    }

    Transition Idle::Run(CleanTankFlow& flow)
    {
        const GPIO::PinState lowPinState = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankLowSensorPin);
        if(lowPinState == GPIO::PinState::Default)
        {
            Logging::Get(flow.GetName())->warn("unable to get pin state for low clean water tank sensor");
            return Transition::Stay();
        }

        // The tank needs filling once the low sensor has read dry for the debounce time.
        if(lowPinState.Triggered || !IsSettled(lowPinState, flow.settings->CleanTank.SensorDebounce))
        {
            return Transition::Stay();
        }

        const GPIO::PinState highPinState = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankHighSensorPin);
        if(highPinState == GPIO::PinState::Default)
        {
            Logging::Get(flow.GetName())->warn("unable to get pin state for high clean water tank sensor");
            return Transition::Stay();
        }

        // Wet at the top but dry at the bottom can't be right, so don't fill on it.
        if(highPinState.Triggered)
        {
            if(!sensorConflictLogged)
            {
                Logging::Get(flow.GetName())->warn("high sensor reads wet but low sensor reads dry; check the sensors and their npn/pnp settings");
                sensorConflictLogged = true;
            }
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