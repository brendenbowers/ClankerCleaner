#include "Flows/CleanTank/CleanTank.h"

namespace ClankerCleaner::Flows::CleanTank
{
    CleanTankFlow::CleanTankFlow(StateMachine::Machine &machine, std::string name)
        : StateMachine::Flow(machine, name)
    {}
}