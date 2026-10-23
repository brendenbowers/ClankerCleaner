// ESP32 settings: there's no config file, so tune these values and rebuild the firmware.

#include "Config/Config.h"

namespace ClankerCleaner::Config
{
    namespace
    {
        constexpr const char* LogLevel = "info";
        constexpr std::uint32_t DirtyTankSensorPin = 22;
        constexpr std::uint32_t PumpRelayPin = 17;
        constexpr bool RelayActiveLow = false;
        constexpr std::chrono::seconds PumpRunTime{240};
        constexpr std::chrono::seconds SensorDebounce{20};
    }

    std::string DefaultPath()
    {
        return {};
    }

    Settings Load(const std::string& /*path*/, bool /*mustExist*/)
    {
        Settings settings;
        settings.Log.Level = LogLevel;
        settings.Gpio.DirtyTankSensorPin = DirtyTankSensorPin;
        settings.Gpio.PumpRelayPin = PumpRelayPin;
        settings.Gpio.RelayActiveLow = RelayActiveLow;
        settings.DirtyTank.PumpRunTime = PumpRunTime;
        settings.DirtyTank.SensorDebounce = SensorDebounce;
        return settings;
    }
}
