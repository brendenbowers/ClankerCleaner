#pragma once

namespace ClankerCleaner::StateMachine::Context
{
    struct ReturnToIdle
    {
    private:
        ReturnToIdle() = default;

    public:
        // Only declared here: the type isn't complete until the closing brace.
        static const ReturnToIdle Default;
    };

    inline const ReturnToIdle ReturnToIdle::Default{};
}