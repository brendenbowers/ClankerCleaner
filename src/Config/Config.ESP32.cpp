// ESP32 settings: there's no config file, so tune these values and rebuild the firmware.
// Credentials come from esp32/Secrets.h.

#include "Config/Config.h"
#include "Config/Secrets.ESP32.h"

#include <cstdio>

#include <esp_mac.h>

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
        constexpr std::chrono::seconds PumpRunTime{90};
        constexpr std::chrono::seconds FullTankExtraPumpTime{30};
        constexpr std::chrono::seconds MopWashWetWindow{15};
        constexpr std::chrono::seconds MaxPumpRunTime{600};
        constexpr std::chrono::seconds RinseTime{7};
        constexpr std::chrono::seconds RinsePumpRunTime{45};
        constexpr std::chrono::seconds SensorDebounce{10};
        constexpr std::chrono::seconds CleanTankSensorDebounce{10};
        constexpr std::chrono::seconds CleanTankFillRunTime{60};
        constexpr std::chrono::seconds CleanTankDetergentRunTime{15};

        // The last half of the WiFi MAC address, e.g. "f1af38": tells boards apart.
        std::string BoardId()
        {
            std::uint8_t mac[6] = {};
            esp_read_mac(mac, ESP_MAC_WIFI_STA);
            char id[7];
            std::snprintf(id, sizeof(id), "%02x%02x%02x", mac[3], mac[4], mac[5]);
            return id;
        }
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
        settings.DirtyTank.FullTankExtraPumpTime = FullTankExtraPumpTime;
        settings.DirtyTank.MopWashWetWindow = MopWashWetWindow;
        settings.DirtyTank.MaxPumpRunTime = MaxPumpRunTime;
        settings.DirtyTank.RinseTime = RinseTime;
        settings.DirtyTank.RinsePumpRunTime = RinsePumpRunTime;
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
        settings.Mqtt.Host = Secrets::MqttHost;
        settings.Mqtt.Port = Secrets::MqttPort;
        settings.Mqtt.Username = Secrets::MqttUsername;
        settings.Mqtt.Password = Secrets::MqttPassword;
        settings.Mqtt.CaCertificatesPem = Secrets::MqttCaCertificate;
        // A broker drops a client when another connects with the same ID, so each board has
        // its own. Any board but the main one also publishes under its own topics, to keep
        // it from overwriting the main board's retained state.
        const std::string boardId = BoardId();
        settings.Mqtt.ClientId += "-" + boardId;
        const std::string mainBoardId = Secrets::MqttMainBoardId;
        if(!mainBoardId.empty() && boardId != mainBoardId)
        {
            settings.Mqtt.TopicPrefix += "/" + boardId;
        }
        // On once there's a broker to publish to.
        settings.Mqtt.Enabled = !settings.Mqtt.Host.empty();
        return settings;
    }
}
