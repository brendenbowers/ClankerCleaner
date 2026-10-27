#include "Setup/ConfigurePins.h"

#include "GPIO/Pin.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Setup
{
    void ConfigurePins(GPIO::PinInterface& pins, Config::Settings& settings)
    {
        auto gpioLog = Logging::Get("GPIO");

        pins.MonitorPin(settings.Gpio.DirtyTankSensorPin, GPIO::Edge::Falling);
        pins.MonitorPin(settings.Gpio.CleanTankHighSensorPin, GPIO::Edge::Falling);
        pins.AddOutputPin(settings.Gpio.PumpRelayPin, settings.Gpio.RelayActiveLow);
        pins.AddOutputPin(settings.Gpio.DirtyTankRinseRelayPin, settings.Gpio.RelayActiveLow);
        pins.AddOutputPin(settings.Gpio.CleanTankFillRelayPin, settings.Gpio.RelayActiveLow);
        pins.AddOutputPin(settings.Gpio.CleanerDosingRelayPin, settings.Gpio.RelayActiveLow);
        pins.Start();

        for (const auto& [id, pin] : pins.GetInputPins())
        {
            gpioLog->info("Monitoring Pin {}", id);
        }

        for (const auto& [id, pin] : pins.GetOutputPins())
        {
            gpioLog->info("Output Pin {}", id);
        }
    }
}
