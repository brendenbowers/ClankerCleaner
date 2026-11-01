#include "GPIO/InputPin.h"
#include "GPIO/Platform.ESP32.h"

namespace ClankerCleaner::GPIO
{
    void InputPin::Configure(PlatformLineConfig& config) const
    {
        const gpio_num_t gpio = Platform::ToGpio(pinId, false);
        gpio_config_t line{};
        line.pin_bit_mask = 1ULL << pinId;
        line.mode = GPIO_MODE_INPUT;
        line.pull_up_en = bias == Bias::PullUp ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        line.pull_down_en = bias == Bias::PullDown ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE;
        line.intr_type = GPIO_INTR_ANYEDGE;
        config.Lines.push_back({gpio, line});
    }
}
