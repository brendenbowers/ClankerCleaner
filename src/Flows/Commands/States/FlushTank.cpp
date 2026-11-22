#include "Flows/Commands/States/FlushTank.h"

#include "Flows/Commands/Constants.h"
#include "Flows/DirtyTank/Constants.h"
#include "Flows/Commands/Context/FlushTankContext.h"
#include "GPIO/Pin.h"
#include "Logging/Logging.h"
#include "Telemetry/Track.h"

namespace ClankerCleaner::Flows::Commands::States
{
    FlushTank::FlushTank()
        : StateWithFlow<CommandFlow>(Constants::State::FLUSH_TANK_STATE)
    {
    }

    Transition FlushTank::Begin(CommandFlow &flow)
    {
        if(disabled)
        {
            Logging::Get(flow.GetName())->error("FlushTank: disabled due to previous error");
            return Transition::Restart();
        }

        if(flow.dirtyTank && (flow.dirtyTank->GetCurrentPhase() == StatePhase::Running || flow.dirtyTank->GetCurrentPhase() == StatePhase::Begin) && flow.dirtyTank->GetCurrentStateName() != DirtyTank::Constants::State::IDLE_STATE)
        {
            Logging::Get(flow.GetName())->warn("dirty tank is busy in {}; cannot flush", flow.dirtyTank->GetCurrentStateName());
            return Transition::Restart();
        }

        if(flow.dirtyTank && flow.dirtyTank->GetCurrentPhase() != StatePhase::Abort)
        {
            flow.dirtyTank->RequestAbort();
            return Transition::Next(Context::FlushTankContext::Get(true));
        }

        return Transition::Next(Context::FlushTankContext::Get(false));
    }

    Transition FlushTank::Run(CommandFlow &flow)
    {
        Context::FlushTankContext* context = flow.GetContext<Context::FlushTankContext>();
        if(!context)
        {
            Logging::Get(flow.GetName())->error("FlushTank: missing context");
            return Transition::Restart();
        }

        bool highWaterSensed = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin).Triggered;
        bool pumpRunning = flow.pinMonitor->IsOutputActive(flow.settings->Gpio.PumpRelayPin);
        bool rinsing = flow.pinMonitor->IsOutputActive(flow.settings->Gpio.DirtyTankRinseRelayPin);
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();


        if(context->highWater)
        {
            if(rinsing)
            {
                if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.DirtyTankRinseRelayPin, false))
                {
                    Logging::Get(flow.GetName())->error("Could stop rinsing!");
                    flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, true);
                    disabled = true;
                    return Transition::Restart();
                }
            }

            if(context->highWaterPumpStart + flow.settings->DirtyTank.PumpRunTime <= now)
            {
                if(highWaterSensed)
                {
                    Logging::Get(flow.GetName())->error("Could not drain tank before pump timeout");
                    return Transition::Restart();
                }

                context->highWater = false;
                return Transition::Stay();
            }

            if(pumpRunning)
            {
                return Transition::Stay();
            }

            if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, true))
            {
                Logging::Get(flow.GetName())->error("Could start pump to drain high water");
                return Transition::Restart();
            }
            context->highWaterPumpStart = now;
            return Transition::Stay();
        }

        if(!pumpRunning)
        {
            if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, true))
            {
                Logging::Get(flow.GetName())->error("Could start pump to drain high water");
                return Transition::Restart();
            }

            context->flushPumpStart = std::chrono::steady_clock::now();
            return Transition::Stay();
        }

        if(highWaterSensed)
        {
            context->highWater = highWaterSensed;
            if(pumpRunning)
            {
                context->highWaterPumpStart = now;
            }
            return Transition::Stay();
        }

        if(context->flushPumpStart + std::chrono::seconds(60) < now)
        {
            return Transition::Restart();
        }

        if(context->flushPumpStart + std::chrono::seconds(5) > now)
        {
            return Transition::Stay();
        }

        if(!rinsing)
        {
            if(context->flushRinseLastStart != std::chrono::steady_clock::time_point{} && 
                context->flushRinseLastStart + std::chrono::seconds(10) > now)
            {
                return Transition::Stay();
            }

            if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.DirtyTankRinseRelayPin, true))
            {
                Logging::Get(flow.GetName())->error("Could start water rinse");
                return Transition::Restart();
            }

            context->flushRinseLastStart = now;
            return Transition::Stay();
        }

        if(context->flushRinseLastStart + std::chrono::seconds(5) < now)
        {
            if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.DirtyTankRinseRelayPin, false))
            {
                Logging::Get(flow.GetName())->error("Could stop water rinse");
                return Transition::Restart();
            }
        }

        return Transition::Stay();
    }

    bool FlushTank::End(CommandFlow & flow)
    {
        disabled = disabled || Cleanup(flow);
        return true;
    }

    void FlushTank::Abort(CommandFlow & flow)
    {
        disabled = disabled || Cleanup(flow);
    }

    bool FlushTank::Cleanup(CommandFlow & flow)
    {
        bool highWaterSensed = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin).Triggered;
        bool ret = flow.pinMonitor->SetOutput(flow.settings->Gpio.DirtyTankRinseRelayPin, false) && !highWaterSensed;
        if(ret)
        {
            ret |= flow.pinMonitor->SetOutput(flow.settings->Gpio.PumpRelayPin, false);
        }

        Context::FlushTankContext* context = flow.GetContext<Context::FlushTankContext>();
        if(context && context->WasDirtyTankFlowRunning())
        {
            ret |= flow.dirtyTank->RequestStart();
        }
        return ret;
    }
}
