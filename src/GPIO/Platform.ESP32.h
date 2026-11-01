#pragma once

// ESP32 platform types (ESP-IDF GPIO driver). Include only from *.ESP32.cpp files.

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include <driver/gpio.h>

#include "GPIO/Pin.h"

namespace ClankerCleaner::GPIO
{
    // One gpio_config_t per pin, applied by PinInterface::PlatformStart.
    struct PlatformLineConfig
    {
        struct Line
        {
            gpio_num_t Pin;
            gpio_config_t Config;
            // Outputs only: the level set before the pin is enabled, so a relay never blips on.
            int InitialLevel = 0;
        };

        std::vector<Line> Lines;
    };

    namespace Platform
    {
        // Pin ids are GPIO numbers; throws for one the chip doesn't have, or that can't drive.
        inline gpio_num_t ToGpio(std::uint32_t pinId, bool output)
        {
            const gpio_num_t gpio = static_cast<gpio_num_t>(pinId);
            if(output ? !GPIO_IS_VALID_OUTPUT_GPIO(gpio) : !GPIO_IS_VALID_GPIO(gpio))
            {
                throw std::invalid_argument("GPIO" + std::to_string(pinId) + " can't be used as an " + (output ? "output" : "input"));
            }
            return gpio;
        }
    }
}
