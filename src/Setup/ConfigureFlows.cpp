#include "Setup/ConfigureFlows.h"

#include <string>
#include <string_view>

#include "Flows/DirtyTank/Constants.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/States/EmptyTank.h"
#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/States/Rinse.h"

#include "Flows/CleanTank/Constants.h"
#include "Flows/CleanTank/CleanTank.h"
#include "Flows/CleanTank/States/Idle.h"
#include "Flows/CleanTank/States/FillTank.h"
#include "Flows/CleanTank/States/AddCleaner.h"

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
        {
            namespace DirtyTank = Flows::DirtyTank;

            DirtyTank::DirtyTankFlow& flow = machine.AddFlow<DirtyTank::DirtyTankFlow>(DirtyTank::Constants::State::FLOWNAME);
            flow.pinMonitor = &pins;
            flow.settings = &settings;
            flow.dockMonitor = dockMonitor;
            flow.AddState<DirtyTank::States::Idle>();
            flow.AddState<DirtyTank::States::EmptyTank>(DirtyTank::Constants::State::EMPTY_TANK_STATE,
                                                                settings.DirtyTank.PumpRunTime);
            flow.AddState<DirtyTank::States::Rinse>();
            flow.AddState<DirtyTank::States::EmptyTank>(DirtyTank::Constants::State::EMPTY_RINSED_STATE,
                                                                settings.DirtyTank.RinsePumpRunTime);
            flow.SetLooping(true);
            LogPhases(flow);
        }
        
        {
            namespace CleanTank = Flows::CleanTank;
            
            CleanTank::CleanTankFlow& flow = machine.AddFlow<CleanTank::CleanTankFlow>(CleanTank::Constants::State::FLOWNAME);
            flow.pinMonitor = &pins;
            flow.settings = &settings;
            flow.dockMonitor = dockMonitor;

            flow.AddState<CleanTank::States::Idle>();
            flow.AddState<CleanTank::States::FillTank>();
            flow.AddState<CleanTank::States::AddCleanerState>();
            
            flow.SetLooping(true);
            LogPhases(flow);
        }
    }
}
