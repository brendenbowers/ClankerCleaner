#pragma once

// Raspberry Pi platform types (libgpiod 2.x). Include only from *.RPi.cpp files.

#include <gpiod.hpp>

#include "GPIO/Pin.h"

namespace ClankerCleaner::GPIO
{
    struct PlatformLineConfig
    {
        gpiod::line_config Lines;
    };

    namespace Platform
    {
        constexpr const char* ChipPath = "/dev/gpiochip0";
        constexpr const char* Consumer = "ClankerCleaner";
    }
}
