#include "GPIO/OutputPin.h"
#include "GPIO/Platform.ESP32.h"

namespace ClankerCleaner::GPIO
{
    void OutputPin::Configure(PlatformLineConfig& config) const
    {
        const gpio_num_t gpio = Platform::ToGpio(pinId, true);
        gpio_config_t line{};
        line.pin_bit_mask = 1ULL << pinId;
        line.mode = GPIO_MODE_OUTPUT;
        line.pull_up_en = GPIO_PULLUP_DISABLE;
        line.pull_down_en = GPIO_PULLDOWN_DISABLE;
        line.intr_type = GPIO_INTR_DISABLE;
        config.Lines.push_back({gpio, line, activeLow ? 1 : 0});
    }
}
