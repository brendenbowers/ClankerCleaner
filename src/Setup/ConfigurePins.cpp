#include "Setup/ConfigurePins.h"

#include "GPIO/InputPin.h"
#include "GPIO/Pin.h"
#include "Logging/Logging.h"

namespace ClankerCleaner::Setup
{
    namespace
    {
        // Either way Triggered means wet: an NPN sensor pulls the line LOW, a PNP one drives it HIGH.
        void MonitorSensor(GPIO::PinInterface& pins, std::uint32_t pin, Config::SensorOutput output)
        {
            if(output == Config::SensorOutput::Pnp)
            {
                pins.MonitorPin(GPIO::InputPin(pin, GPIO::Edge::Rising, GPIO::Bias::PullDown));
            }
            else
            {
                pins.MonitorPin(GPIO::InputPin(pin, GPIO::Edge::Falling, GPIO::Bias::PullUp));
            }
        }
    }

    void ConfigurePins(GPIO::PinInterface& pins, Config::Settings& settings)
    {
        auto gpioLog = Logging::Get("GPIO");

        MonitorSensor(pins, settings.Gpio.DirtyTankSensorPin, settings.Gpio.DirtyTankSensorOutput);
        MonitorSensor(pins, settings.Gpio.CleanTankHighSensorPin, settings.Gpio.CleanTankHighSensorOutput);
        MonitorSensor(pins, settings.Gpio.CleanTankLowSensorPin, settings.Gpio.CleanTankLowSensorOutput);
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
