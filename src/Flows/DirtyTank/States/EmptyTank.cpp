#include "Flows/DirtyTank/States/EmptyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "GPIO/Constants.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{
    EmptyTank::EmptyTank(const std::string name, std::chrono::seconds runTime)
        : StateWithFlow<DirtyTankFlow>(name)
        , runTime(runTime)
    {
    }

    Transition EmptyTank::Begin(DirtyTankFlow& flow)
    {
        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, true))
        {
            Logging::Get(flow.GetName())->error("couldn't turn the pump on");
            return Transition::Abort();
        }

        Logging::Get(flow.GetName())->info("pump on, expected runtime: {} s", runTime.count());
        pumpStart = std::chrono::steady_clock::now();
        return Transition::Next();
    }

    Transition EmptyTank::Run(DirtyTankFlow& flow)
    {
        //todo: add checking the low sensor on the dirty tank once we have one
        if(pumpStart.time_since_epoch().count() == 0 || std::chrono::steady_clock::now() - pumpStart >= runTime)
        {
            GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin);
            if(state.PinId != 0 && state.Triggered != false)
            {
                Logging::Get(flow.GetName())->error("dirty tank sensor still wet after {} s", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - pumpStart).count());
                return Transition::Abort();
            }

            return Transition::Next();
        }

        return Transition::Stay();
    }

    bool EmptyTank::End(DirtyTankFlow& flow)
    {
        return StopPumping(flow);
    }

    void EmptyTank::Abort(DirtyTankFlow& flow)
    {
        StopPumping(flow);
    }

    bool EmptyTank::StopPumping(DirtyTankFlow& flow)
    {
        if(flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, false))
        {
            const auto ranFor = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - pumpStart);
            Logging::Get(flow.GetName())->info("pump off after {} s", ranFor.count());
            pumpStart = std::chrono::steady_clock::time_point();
            return true;
        }

        Logging::Get(flow.GetName())->error("couldn't turn the pump off");
        return false;
    }
}