#include "Flows/Commands/Commands.h"

namespace ClankerCleaner::Flows::Commands
{
    CommandFlow::CommandFlow(StateMachine::Machine& machine, std::string name)
        : StateMachine::Flow(machine, name)
    {}
}
