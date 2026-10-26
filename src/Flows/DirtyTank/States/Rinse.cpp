#include "Flows/DirtyTank/States/Rinse.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"

namespace ClankerCleaner::Flows::DirtyTank::States
{
    Rinse::Rinse()
        : StateMachine::State(Constants::State::RINSE_TANK_STATE)
    {
    }

    Transition Rinse::Begin(Flow &flow)
    {
        return Transition::Abort();
    }

    Transition Rinse::Run(Flow &flow)
    {
        return Transition::Abort();
    }
    
    bool Rinse::End(Flow &flow)
    {
        return false;
    }

    void Rinse::Abort(Flow &flow)
    {
    }
}