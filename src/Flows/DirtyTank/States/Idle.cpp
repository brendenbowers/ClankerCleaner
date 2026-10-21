#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{
    Idle::Idle()
        : State(Constants::State::IDLE_STATE)
        , lastEvent(0)
    {
    }

    Transition Idle::Run(Flow &flow)
    {
        DirtyTankFlow& dirtyTankFlow = static_cast<DirtyTankFlow&>(flow);

        // todo: maybe an optimization checking first if there are changes
        // GetSince includes events at exactly lastEvent, so +1 skips the one already handled.
        std::vector<GPIO::PinState> changedPins = dirtyTankFlow.pinMonitor->GetSince(lastEvent + 1);

        auto it = std::find_if(changedPins.begin(), changedPins.end(), [](GPIO::PinState pinState) { return pinState.PinId == GPIO::Constants::HIGH_WASTE_WATER_TANK; });
        if(it == changedPins.end())
        {
            return Transition::Stay();
        }

        GPIO::PinState& pinState = *it;
        lastEvent = pinState.ChangedNS;

        if(pinState.Triggered)
        {
            return Transition::Next();
        }

        return Transition::Stay();        
    }
}