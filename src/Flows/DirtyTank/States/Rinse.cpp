#include "Flows/DirtyTank/States/Rinse.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{
    Rinse::Rinse()
        : StateMachine::StateWithFlow<Flows::DirtyTank::DirtyTankFlow>(Constants::State::RINSE_TANK_STATE)
    {
    }

    Transition Rinse::Begin(Flows::DirtyTank::DirtyTankFlow& flow)
    {
        GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin);
        if(state.PinId != 0 && state.Triggered != false)
        {
            Logging::Get(flow.GetName())->error("dirty tank sensor is wet cannot rinse due to water level");
            return Transition::Abort();
        }

        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.DirtyTankRinseRelayPin, true))
        {
            Logging::Get(flow.GetName())->error("couldn't rinse the dirty tank");
            return Transition::Abort();
        }

        Logging::Get(flow.GetName())->info("rinsing dirty tank");
        rinseStart = std::chrono::steady_clock::now();
        return Transition::Next();
    }

    Transition Rinse::Run(Flows::DirtyTank::DirtyTankFlow& flow)
    {
        GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin);
        if(state.PinId != 0 && state.Triggered != false)
        {
            Logging::Get(flow.GetName())->error("dirty tank sensor is wet after {} s of rinsing", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - rinseStart).count());
            return Transition::Abort();
        }

        if(rinseStart.time_since_epoch().count() == 0 || std::chrono::steady_clock::now() - rinseStart >= flow.settings->DirtyTank.RinseTime)
        {
            return Transition::Next();
        }


        return Transition::Stay();
    }
    
    bool Rinse::End(Flows::DirtyTank::DirtyTankFlow& flow)
    {
        return StopRinsing(flow);
    }

    void Rinse::Abort(Flows::DirtyTank::DirtyTankFlow& flow)
    {
        StopRinsing(flow);
    }
    
    bool Rinse::StopRinsing(Flows::DirtyTank::DirtyTankFlow& flow)
    {
        rinseStart =  std::chrono::steady_clock::time_point();
        
        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.DirtyTankRinseRelayPin, false))
        {
            Logging::Get(flow.GetName())->error("couldn't stop rinsing the dirty tank");
            return false;
        }

        return true;
    }
}