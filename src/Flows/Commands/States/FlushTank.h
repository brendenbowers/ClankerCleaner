#pragma once

#include <string>

#include "StateMachine/State.h"
#include "Flows/Commands/Commands.h"

namespace ClankerCleaner::Flows::Commands::States
{
    using namespace ClankerCleaner::StateMachine;

    /// @brief handles each command as it arrives, for as long as the flow runs
    class FlushTank : public StateWithFlow<CommandFlow>
    {
    public:
        FlushTank();
        ~FlushTank() override = default;

        Transition Begin(CommandFlow& flow) override;
        Transition Run(CommandFlow& flow) override;
        bool End(CommandFlow& flow) override;
        void Abort(CommandFlow& flow) override;

    private:
        bool Cleanup(CommandFlow& flow);

        bool disabled = false;
    };
}
