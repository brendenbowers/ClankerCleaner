// ESP32 settings: there's no config file, so tune these values and rebuild the firmware.
// Credentials come from esp32/Secrets.h.

#include "Config/Config.h"
#include "Config/Secrets.ESP32.h"

namespace ClankerCleaner::Config
{
    namespace
    {
        // ESP32-S3 GPIO numbers (Freenove ESP32-S3 WROOM, N16R8). These avoid the strapping
        // pins (0, 3, 45, 46), USB (19, 20), the serial console (43, 44), flash and octal
        // PSRAM (26 to 37), the onboard LED (2) and the camera connector's pins.
        constexpr std::uint32_t DirtyTankSensorPin = 1;
        constexpr std::uint32_t CleanTankHighSensorPin = 42;
        constexpr std::uint32_t CleanTankLowSensorPin = 21;
        constexpr std::uint32_t PumpRelayPin = 38;
        constexpr std::uint32_t DirtyTankRinseRelayPin = 39;
        constexpr std::uint32_t CleanTankFillRelayPin = 40;
        constexpr std::uint32_t CleanerDosingRelayPin = 41;

        constexpr const char* LogLevel = "info";
        constexpr bool RelayActiveLow = false;
        constexpr std::chrono::seconds PumpRunTime{240};
        constexpr std::chrono::seconds RinseTime{15};
        constexpr std::chrono::seconds RinsePumpRunTime{60};
        constexpr std::chrono::seconds FullOperationTime{600};
        constexpr std::chrono::seconds SensorDebounce{20};
        constexpr std::chrono::seconds CleanTankSensorDebounce{20};
        constexpr std::chrono::seconds CleanTankFillRunTime{120};
        constexpr std::chrono::seconds CleanTankDetergentRunTime{9};
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
        settings.Gpio.CleanTankHighSensorPin = CleanTankHighSensorPin;
        settings.Gpio.CleanTankLowSensorPin = CleanTankLowSensorPin;
        settings.Gpio.DirtyTankSensorOutput = SensorOutput::Npn;
        settings.Gpio.CleanTankHighSensorOutput = SensorOutput::Pnp;
        settings.Gpio.CleanTankLowSensorOutput = SensorOutput::Pnp;
        settings.Gpio.PumpRelayPin = PumpRelayPin;
        settings.Gpio.DirtyTankRinseRelayPin = DirtyTankRinseRelayPin;
        settings.Gpio.CleanTankFillRelayPin = CleanTankFillRelayPin;
        settings.Gpio.CleanerDosingRelayPin = CleanerDosingRelayPin;
        settings.Gpio.RelayActiveLow = RelayActiveLow;
        settings.DirtyTank.PumpRunTime = PumpRunTime;
        settings.DirtyTank.RinseTime = RinseTime;
        settings.DirtyTank.RinsePumpRunTime = RinsePumpRunTime;
        settings.DirtyTank.FullOperationTime = FullOperationTime;
        settings.DirtyTank.SensorDebounce =SensorDebounce;
        settings.CleanTank.SensorDebounce = CleanTankSensorDebounce;
        settings.CleanTank.FillRunTime = CleanTankFillRunTime;
        settings.CleanTank.DetergentRunTime = CleanTankDetergentRunTime;
        settings.Dreame.Account = Secrets::DreameAccount;
        settings.Dreame.Username = Secrets::DreameUsername;
        settings.Dreame.Password = Secrets::DreamePassword;
        settings.Dreame.Country = Secrets::DreameCountry;
        settings.Dreame.Device = Secrets::DreameDevice;
        // On once there's an account to sign in to.
        settings.Dreame.Enabled = !settings.Dreame.Username.empty();
        return settings;
    }
}
