#pragma once

namespace ClankerCleaner::Flows::DirtyTank::Constants
{
    namespace State 
    {
        constexpr const char* FLOWNAME = "DirtyTankFlow";
        constexpr const char* IDLE_STATE = "DIRTY_TANK_IDLE";
        constexpr const char* EMPTY_TANK_STATE = "DIRTY_TANK_EMPTY";
        constexpr const char* RINSE_TANK_STATE = "RINSE_DIRTY_TANK";
        constexpr const char* EMPTY_RINSED_STATE = "EMPTY_RINSED_TANK";

    }
}