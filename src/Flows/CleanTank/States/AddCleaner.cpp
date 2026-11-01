#include "Flows/CleanTank/States/AddCleaner.h"
#include "Flows/CleanTank/CleanTank.h"
#include "Flows/CleanTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"
#include "FillTank.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Flows::CleanTank::States
{
    AddCleanerState::AddCleanerState()
        : StateWithFlow<CleanTankFlow>(Constants::State::ADD_CLEANER_STATE)
    {
    }

    Transition AddCleanerState::Begin(CleanTankFlow &flow)
    {
        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.CleanerDosingRelayPin, true))
        {
            Logging::Get(flow.GetName())->error("couldn't activate detergent fill relay");
            return Transition::Abort();
        }

        fillStart = std::chrono::steady_clock::now();
        return Transition::Next();
    }

    Transition AddCleanerState::Run(CleanTankFlow & flow)
    {

        if(std::chrono::steady_clock::now() < fillStart + flow.settings->CleanTank.DetergentRunTime)
        {
            return Transition::Stay();
        }

        return Transition::Next();
    }

    bool AddCleanerState::End(CleanTankFlow & flow)
    {
        return StopAddingDetergent(flow);
    }

    void AddCleanerState::Abort(CleanTankFlow & flow)
    {
        StopAddingDetergent(flow);
    }

    bool AddCleanerState::StopAddingDetergent(CleanTankFlow & flow)
    {
        if(!flow.pinMonitor->SetOutput(flow.settings->Gpio.CleanerDosingRelayPin, false))
        {
            Logging::Get(flow.GetName())->error("couldn't deactivate detergent fill relay");
            return false;
        }

        return true;
    }
}
