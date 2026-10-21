#include "Flows/DirtyTank/States/EmptyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{
    namespace _Priv
    {
        // todo: make this configurable
        constexpr std::chrono::steady_clock::duration PumpRunTime = std::chrono::minutes(4);
    }

    EmptyTank::EmptyTank()
        : StateMachine::State(Constants::State::EMPTY_TANK_STATE)
    {
    }

    Transition EmptyTank::Begin(Flow &flow)
    {
        DirtyTankFlow& dirtyTankFlow = static_cast<DirtyTankFlow&>(flow);

        if(!dirtyTankFlow.pinMonitor->SetOutput(Constants::State::PUMP_RELAY_PIN, true))
        {
            Logging::Get(flow.GetName())->error("couldn't turn the pump on");
            return Transition::Abort();
        }

        Logging::Get(flow.GetName())->info("pump on");
        pumpStart = std::chrono::steady_clock::now();
        return Transition::Next();
    }

    Transition EmptyTank::Run(Flow & /*flow*/)
    {

        //todo: add checking the low sensor on the dirty tank once we have one
        if(pumpStart.time_since_epoch().count() == 0 || std::chrono::steady_clock::now() - pumpStart >= _Priv::PumpRunTime)
        {
            return Transition::Next();
        }

        return Transition::Stay();
    }

    bool EmptyTank::End(Flow &flow)
    {
        return StopPumping(flow);
    }

    void EmptyTank::Abort(Flow &flow)
    {
        StopPumping(flow);
    }

    bool EmptyTank::StopPumping(Flow &flow)
    {
        DirtyTankFlow& dirtyTankFlow = static_cast<DirtyTankFlow&>(flow);
        if(dirtyTankFlow.pinMonitor->SetOutput(Constants::State::PUMP_RELAY_PIN, false))
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