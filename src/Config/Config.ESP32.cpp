// ESP32 settings: there's no config file, so tune these values and rebuild the firmware.

#include "Config/Config.h"
#include "GPIO/Constants.h"

namespace ClankerCleaner::Config
{
    namespace
    {
        constexpr const char* LogLevel = "info";
        constexpr bool RelayActiveLow = false;
        constexpr std::chrono::seconds PumpRunTime{240};
        constexpr std::chrono::seconds RinseTime{15};
        constexpr std::chrono::seconds RinsePumpRunTime{60};
        constexpr std::chrono::seconds SensorDebounce{20};
        // The cloud client isn't ported to the ESP32 yet.
        constexpr bool DreameEnabled = false;
    }

    std::string DefaultPath()
    {
        return {};
    }

    Settings Load(const std::string& /*path*/, bool /*mustExist*/)
    {
        Settings settings;
        settings.Log.Level = LogLevel;
        settings.Gpio.DirtyTankSensorPin = GPIO::Constants::HIGH_WASTE_WATER_TANK;
        settings.Gpio.CleanTankHighSensorPin = GPIO::Constants::HIGH_CLEAN_WATER_TANK;
        settings.Gpio.PumpRelayPin = GPIO::Constants::PUMP_RELAY_PIN;
        settings.Gpio.DirtyTankRinseRelayPin = GPIO::Constants::DIRTY_TANK_RINSE_RELAY_PIN;
        settings.Gpio.CleanTankFillRelayPin = GPIO::Constants::CLEAN_TANK_FILL_RELAY_PIN;
        settings.Gpio.CleanerDosingRelayPin = GPIO::Constants::CLEANER_DOSING_RELAY_PIN;
        settings.Gpio.RelayActiveLow = RelayActiveLow;
        settings.DirtyTank.PumpRunTime = PumpRunTime;
        settings.DirtyTank.RinseTime = RinseTime;
        settings.DirtyTank.RinsePumpRunTime = RinsePumpRunTime;
        settings.DirtyTank.SensorDebounce = SensorDebounce;
        settings.Dreame.Enabled = DreameEnabled;
        return settings;
    }
}
