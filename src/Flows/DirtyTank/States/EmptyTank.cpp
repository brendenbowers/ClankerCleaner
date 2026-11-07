#include "Flows/DirtyTank/States/EmptyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "StateMachine/Context/ReturnToIdle.h"
#include "GPIO/Constants.h"
#include "GPIO/Pin.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{
    EmptyTank::EmptyTank(const std::string name, std::chrono::seconds runTime, std::chrono::seconds fullTankExtraTime)
        : StateWithFlow<DirtyTankFlow>(name)
        , runTime(runTime)
        , fullTankExtraTime(fullTankExtraTime)
        , currentRunTime(runTime)
    {
    }

    Transition EmptyTank::Begin(DirtyTankFlow& flow)
    {
        const GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin);
        const bool full = state != GPIO::PinState::Default && state.Triggered;
        currentRunTime = full ? runTime + fullTankExtraTime : runTime;
        keepingUpLogged = false;

        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, true))
        {
            Logging::Get(flow.GetName())->error("couldn't turn the pump on");
            return Transition::Abort();
        }

        if(full && fullTankExtraTime.count() > 0)
        {
            Logging::Get(flow.GetName())->info("pump on, tank full, expected runtime: {} s ({} s + {} s)", currentRunTime.count(),
                                               runTime.count(), fullTankExtraTime.count());
        }
        else
        {
            Logging::Get(flow.GetName())->info("pump on, expected runtime: {} s", currentRunTime.count());
        }
        pumpStart = std::chrono::steady_clock::now();
        return Transition::Next();
    }

    Transition EmptyTank::Run(DirtyTankFlow& flow)
    {
        Transition next = flow.HasContextOfType<StateMachine::Context::ReturnToIdle>()
            ? Transition::Restart()
            : Transition::Next();

        //todo: add checking the low sensor on the dirty tank once we have one
        if(pumpStart.time_since_epoch().count() == 0 || std::chrono::steady_clock::now() - pumpStart >= currentRunTime)
        {
            GPIO::PinState state = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin);
            const bool wet = state != GPIO::PinState::Default && state.Triggered;

            // The dock drains into the tank throughout a mop wash, so keep up with it while the
            // sensor is wet or went dry only recently.
            const std::chrono::seconds wetWindow = flow.settings->DirtyTank.MopWashWetWindow;
            const std::chrono::steady_clock::time_point dryAt{std::chrono::nanoseconds(state.ChangedNS)};
            const bool recentlyWet = state != GPIO::PinState::Default && !state.Triggered && dryAt + wetWindow > std::chrono::steady_clock::now();
            if(flow.dock.IsWashingMop() && (wet || recentlyWet))
            {
                const std::chrono::seconds maxRunTime = flow.settings->DirtyTank.MaxPumpRunTime;
                if(std::chrono::steady_clock::now() - pumpStart < maxRunTime)
                {
                    if(!keepingUpLogged)
                    {
                        Logging::Get(flow.GetName())->info("mops are being washed; pumping until the sensor has been dry for {} s", wetWindow.count());
                        keepingUpLogged = true;
                    }
                    return Transition::Stay();
                }

                // Then on as if the wash were over: stop, or abort if the tank is still wet.
                Logging::Get(flow.GetName())->warn("mops still being washed, but the pump has reached its {} s limit", maxRunTime.count());
            }

            if(wet)
            {
                Logging::Get(flow.GetName())->error("dirty tank sensor still wet after {} s", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - pumpStart).count());
                return Transition::Abort();
            }

            return next;
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