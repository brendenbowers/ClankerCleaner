#pragma once

namespace ClankerCleaner::Flows::CleanTank::Constants
{
    namespace State 
    {
        constexpr const char* FLOWNAME = "CleanTankFlow";
        constexpr const char* IDLE_STATE = "CLEAN_TANK_IDLE";
        constexpr const char* FILL_TANK_STATE = "FILL_TANK";
        constexpr const char* ADD_CLEANER_STATE = "ADD_CLEANER";
    }
}