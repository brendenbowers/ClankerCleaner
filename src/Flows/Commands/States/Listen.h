#pragma once

#include <string>

#include "StateMachine/State.h"
#include "Flows/Commands/Commands.h"

namespace ClankerCleaner::Flows::Commands::States
{
    using namespace ClankerCleaner::StateMachine;

    /// @brief handles each command as it arrives, for as long as the flow runs
    class Listen : public StateWithFlow<CommandFlow>
    {
    public:
        Listen();
        ~Listen() override = default;

        Transition Run(CommandFlow& flow) override;

    private:
        // Why the command can't run; empty once it has been passed on.
        std::string Handle(CommandFlow& flow, const std::string& command);
    };
}
