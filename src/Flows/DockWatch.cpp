#include "Flows/DockWatch.h"

namespace ClankerCleaner::Flows
{
    void DockWatch::Watch(Dreame::DockMonitor& monitor)
    {
        washingMop = monitor.GetStatus().IsWashingMop();
        monitor.AddChangeListener([this](const Dreame::DockStatus& previous, const Dreame::DockStatus& current)
        {
            HandleChange(previous, current);
        });
    }

    void DockWatch::HandleChange(const Dreame::DockStatus& previous, const Dreame::DockStatus& current)
    {
        washingMop = current.IsWashingMop();

        // Latched, so a finished wash isn't lost if another change arrives before a state checks.
        const bool interrupted = current.Robot == Dreame::RobotState::WashingPaused ||
                                 current.Robot == Dreame::RobotState::Paused ||
                                 current.Robot == Dreame::RobotState::Error;
        if(previous.IsWashingMop() && !current.IsWashingMop() && !interrupted)
        {
            washFinished = true;
        }
    }
}
