#include "GPIO/OutputPin.h"

namespace ClankerCleaner::GPIO
{
    OutputPin::OutputPin(std::uint32_t pinId, bool activeLow)
        : Pin(pinId)
        , activeLow(activeLow)
    {
    }
}
