#pragma once

namespace ClankerCleaner::Flows::Commands::Constants
{
    namespace State
    {
        constexpr const char* FLOWNAME = "CommandFlow";
        constexpr const char* LISTEN_STATE = "COMMAND_LISTEN";
    }

    namespace Command
    {
        constexpr const char* DRAIN = "drain";
        constexpr const char* RINSE = "rinse";
        constexpr const char* FILL = "fill";
        constexpr const char* ADD_CLEANER = "add_cleaner";
        constexpr const char* STOP = "stop";
        constexpr const char* RESUME = "resume";
    }
}
