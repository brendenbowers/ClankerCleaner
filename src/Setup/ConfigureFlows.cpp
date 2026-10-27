#include "Setup/ConfigureFlows.h"

#include <string>
#include <string_view>

#include "Flows/DirtyTank/Constants.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/States/EmptyTank.h"
#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/States/Rinse.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Setup
{
    namespace
    {
        std::string_view PhaseName(StateMachine::StatePhase phase)
        {
            using StateMachine::StatePhase;
            switch (phase)
            {
            case StatePhase::None: return "none";
            case StatePhase::Begin: return "begin";
            case StatePhase::Running: return "running";
            case StatePhase::End: return "end";
            case StatePhase::Abort: return "abort";
            }
            return "unknown";
        }

        // Logs every phase change to the flow's own logger: entering a state at info,
        // aborts at warn with the reason, everything else at debug.
        void LogPhases(StateMachine::Flow& flow)
        {
            using StateMachine::Flow;
            using StateMachine::State;
            using StateMachine::StatePhase;

            auto flowLogger = Logging::Get(flow.GetName());
            flow.SetPhaseListener([flowLogger](const Flow& changedFlow, const State& state, StatePhase phase)
            {
                if (phase == StatePhase::Abort)
                {
                    const std::string reason = changedFlow.GetLastError();
                    flowLogger->warn("{} aborted{}{}", state.GetName(), reason.empty() ? "" : ": ", reason);
                }
                else if (phase == StatePhase::Begin)
                {
                    flowLogger->info("entering {}", state.GetName());
                }
                else
                {
                    flowLogger->debug("{} {}", state.GetName(), PhaseName(phase));
                }
            });
        }
    }

    void ConfigureFlows(StateMachine::Machine& machine, GPIO::PinInterface& pins, Config::Settings& settings,
                        Dreame::DockMonitor* dockMonitor)
    {
        namespace DirtyTank = Flows::DirtyTank;

        DirtyTank::DirtyTankFlow& dirtyTankFlow = machine.AddFlow<DirtyTank::DirtyTankFlow>(DirtyTank::Constants::State::FLOWNAME);
        dirtyTankFlow.pinMonitor = &pins;
        dirtyTankFlow.settings = &settings;
        dirtyTankFlow.dockMonitor = dockMonitor;
        dirtyTankFlow.AddState<DirtyTank::States::Idle>();
        dirtyTankFlow.AddState<DirtyTank::States::EmptyTank>(DirtyTank::Constants::State::EMPTY_TANK_STATE,
                                                             settings.DirtyTank.PumpRunTime);
        dirtyTankFlow.AddState<DirtyTank::States::Rinse>();
        dirtyTankFlow.AddState<DirtyTank::States::EmptyTank>(DirtyTank::Constants::State::EMPTY_RINSED_STATE,
                                                             settings.DirtyTank.RinsePumpRunTime);
        dirtyTankFlow.SetLooping(true);
        LogPhases(dirtyTankFlow);
    }
}
