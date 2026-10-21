#include "Flows/DirtyTank/DirtyTank.h"

namespace ClankerCleaner::Flows::DirtyTank
{
    DirtyTankFlow::DirtyTankFlow(StateMachine::Machine &machine, std::string name)
        : StateMachine::Flow(machine, name)
    {}
}