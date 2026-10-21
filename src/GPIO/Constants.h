#pragma once


#include <cstdint>

namespace ClankerCleaner::GPIO::Constants
{
    constexpr const char* CONSUMER = "ClankerCleaner";
    constexpr const char* CHIP = "/dev/gpiochip0";
    constexpr std::uint32_t HIGH_WASTE_WATER_TANK = 22;
}