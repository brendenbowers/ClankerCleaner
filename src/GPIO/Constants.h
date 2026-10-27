#pragma once


#include <cstdint>

namespace ClankerCleaner::GPIO::Constants
{
    constexpr std::uint32_t HIGH_WASTE_WATER_TANK = 22;
    constexpr std::uint32_t PUMP_RELAY_PIN = 17;
    constexpr std::uint32_t DIRTY_TANK_RINSE_RELAY_PIN = 27;
    constexpr std::uint32_t CLEAN_TANK_FILL_RELAY_PIN = 5;
    constexpr std::uint32_t CLEANER_DOSING_RELAY_PIN = 6;
}
