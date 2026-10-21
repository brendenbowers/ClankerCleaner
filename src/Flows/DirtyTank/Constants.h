#pragma once

namespace ClankerCleaner::Flows::DirtyTank::Constants
{
    namespace State 
    {
        constexpr const char* IDLE_STATE = "DIRTY_TANK_IDLE";
        constexpr const char* EMPTY_TANK_STATE = "DIRTY_TANK_EMPTY";

        constexpr std::uint32_t PUMP_RELAY_PIN = 17;
    }
}