#include "Flows/Commands/States/Listen.h"

#include <optional>

#include "Flows/CleanTank/Constants.h"
#include "Flows/Commands/Constants.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "Logging/Logging.h"
#include "Telemetry/Track.h"

namespace ClankerCleaner::Flows::Commands::States
{
    namespace
    {
        // Why tank can't take a command; empty if it's waiting in its idle state. Past Running,
        // Idle has already handed over to the next state.
        std::string WhyNotIdle(const StateMachine::Flow& tank, const char* idleState)
        {
            if(!tank.IsRunning())
            {
                return tank.GetName() + " is stopped; send resume first";
            }
            const std::string state = tank.GetCurrentStateName();
            if(state != idleState || tank.GetCurrentPhase() != StatePhase::Running)
            {
                return tank.GetName() + " is busy in " + state;
            }
            return {};
        }
    }

    Listen::Listen()
        : StateWithFlow<CommandFlow>(Constants::State::LISTEN_STATE)
    {
    }

    Transition Listen::Run(CommandFlow& flow)
    {
        // One per tick, so the tank flows act on each command before the next is checked.
        if(const std::optional<std::string> command = Telemetry::TakeCommand())
        {
            const std::string refused = Handle(flow, *command);
            if(refused.empty())
            {
                Logging::Get(flow.GetName())->info("{}: accepted", *command);
            }
            else
            {
                Logging::Get(flow.GetName())->warn("{}: refused: {}", *command, refused);
            }
            TELEMETRY_TRACK(CommandHandled, *command, refused.empty(), refused);
            flow.Wake();
        }
        return Transition::Stay();
    }

    std::string Listen::Handle(CommandFlow& flow, const std::string& command)
    {
        namespace Command = Constants::Command;
        const char* dirtyIdle = DirtyTank::Constants::State::IDLE_STATE;
        const char* cleanIdle = CleanTank::Constants::State::IDLE_STATE;

        if(command == Command::STOP)
        {
            // Aborting runs each state's Abort, which turns its outputs off.
            flow.dirtyTank->RequestAbort();
            flow.cleanTank->RequestAbort();
            return {};
        }

        if(command == Command::RESUME)
        {
            if(flow.dirtyTank->IsRunning() && flow.cleanTank->IsRunning())
            {
                return "already running";
            }
            // RequestStart does nothing to a flow that's running.
            flow.dirtyTank->RequestStart();
            flow.cleanTank->RequestStart();
            return {};
        }

        if(command == Command::DRAIN)
        {
            std::string busy = WhyNotIdle(*flow.dirtyTank, dirtyIdle);
            if(busy.empty())
            {
                flow.dirtyTank->drainRequested = true;
            }
            return busy;
        }

        if(command == Command::RINSE)
        {
            std::string busy = WhyNotIdle(*flow.dirtyTank, dirtyIdle);
            if(!busy.empty())
            {
                return busy;
            }
            // Rinse would refuse this too, but by aborting, which would stop the automatic runs.
            const GPIO::PinState sensor = flow.pinMonitor->GetState(flow.settings->Gpio.DirtyTankSensorPin);
            if(sensor != GPIO::PinState::Default && sensor.Triggered)
            {
                return "dirty tank sensor is wet; drain it first";
            }
            flow.dirtyTank->rinseRequested = true;
            return {};
        }

        if(command == Command::FILL)
        {
            std::string busy = WhyNotIdle(*flow.cleanTank, cleanIdle);
            if(!busy.empty())
            {
                return busy;
            }
            const GPIO::PinState sensor = flow.pinMonitor->GetState(flow.settings->Gpio.CleanTankHighSensorPin);
            if(sensor == GPIO::PinState::Default)
            {
                return "clean tank high sensor hasn't been read";
            }
            if(sensor.Triggered)
            {
                return "clean tank is already full";
            }
            flow.cleanTank->fillRequested = true;
            return {};
        }

        return "unknown command";
    }
}
