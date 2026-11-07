#include "Flows/CleanTank/States/FillTank.h"
#include "Flows/CleanTank/CleanTank.h"
#include "Flows/CleanTank/Constants.h"
#include "StateMachine/Context/ReturnToIdle.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"
#include "FillTank.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Flows::CleanTank::States
{
    FillTank::FillTank()
        : StateWithFlow<CleanTankFlow>(Constants::State::FILL_TANK_STATE)
    {
    }

    Transition FillTank::Begin(CleanTankFlow& flow)
    {
        GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankHighSensorPin);
        // Back to Idle rather than on to AddCleaner: nothing was filled, so nothing to dose.
        if(state != GPIO::PinState::Default && state.Triggered)
        {
            Logging::Get(flow.GetName())->info("clean tank already full, not filling");
            return Transition::Restart();
        }

        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.CleanTankFillRelayPin, true))
        {
            Logging::Get(flow.GetName())->error("couldn't activate clean water tank fill relay");
            return Transition::Abort();
        }

        fillStart = std::chrono::steady_clock::now();
        return Transition::Next();
    }

    Transition FillTank::Run(CleanTankFlow& flow)
    {    
        Transition next = flow.HasContextOfType<StateMachine::Context::ReturnToIdle>()
            ? Transition::Restart()
            : Transition::Next();

        {
            GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankHighSensorPin);
            if(state != GPIO::PinState::Default && state.Triggered)
            {
                Logging::Get(flow.GetName())->warn("clean tank full while filling");
                return next;
            }
        }
 

        if(std::chrono::steady_clock::now() < (fillStart + flow.settings->CleanTank.FillRunTime))
        {
            return Transition::Stay();
        }

        {
            GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankLowSensorPin);
            if(state != GPIO::PinState::Default && !state.Triggered)
            {
                Logging::Get(flow.GetName())->warn("clean tank did not fill in expected time");
                return Transition::Abort();
            }
        }


        return next;
    }

    bool FillTank::End(CleanTankFlow & flow)
    {
        return StopFillingTank(flow);
    }

    void FillTank::Abort(CleanTankFlow & flow)
    {
        StopFillingTank(flow);
    }

    bool FillTank::StopFillingTank(CleanTankFlow & flow)
    {
        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.CleanTankFillRelayPin, false))
        {
            Logging::Get(flow.GetName())->error("couldn't deactivate clean water tank fill relay");
            return false;
        }

        return true;
    }
}
