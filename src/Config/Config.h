#pragma once

#include <chrono>
#include <cstdint>
#include <string>

#include "GPIO/Constants.h"

// Platform-neutral settings. Config.RPi.cpp reads them from a TOML file; Config.ESP32.cpp
// returns values compiled into the firmware. The member initialisers are the defaults
// used for anything a platform doesn't set.
namespace ClankerCleaner::Config
{
    struct LogSettings
    {
        // trace, debug, info, warn, err, critical or off.
        std::string Level = "info";
        // Rotating log file alongside the console; empty for console only.
        std::string FilePath;
    };

    // How a level sensor's output switches. Npn pulls the line LOW when wet (pull-up to 3.3 V);
    // Pnp drives it HIGH when wet (through a divider that also pulls it down when dry).
    enum class SensorOutput
    {
        Npn,
        Pnp
    };

    struct GpioSettings
    {
        // Line (BCM GPIO) numbers, not physical header pins.
        std::uint32_t DirtyTankSensorPin = GPIO::Constants::HIGH_WASTE_WATER_TANK;
        std::uint32_t CleanTankHighSensorPin = GPIO::Constants::HIGH_CLEAN_WATER_TANK;
        std::uint32_t CleanTankLowSensorPin = GPIO::Constants::LOW_CLEAN_WATER_TANK;
        SensorOutput DirtyTankSensorOutput = SensorOutput::Npn;
        SensorOutput CleanTankHighSensorOutput = SensorOutput::Npn;
        SensorOutput CleanTankLowSensorOutput = SensorOutput::Npn;
        std::uint32_t PumpRelayPin =GPIO::Constants::PUMP_RELAY_PIN;
        std::uint32_t DirtyTankRinseRelayPin = GPIO::Constants::DIRTY_TANK_RINSE_RELAY_PIN;
        std::uint32_t CleanTankFillRelayPin = GPIO::Constants::CLEAN_TANK_FILL_RELAY_PIN;
        std::uint32_t CleanerDosingRelayPin = GPIO::Constants::CLEANER_DOSING_RELAY_PIN;
        // True for a relay board with its trigger jumper on L.
        bool RelayActiveLow = false;
    };

    struct DirtyTankSettings
    {
        std::chrono::seconds PumpRunTime{240};
        // How long the rinse solenoid stays open each time the tank is rinsed.
        std::chrono::seconds RinseTime{15};
        // How long the pump runs to empty the tank after each rinse.
        std::chrono::seconds RinsePumpRunTime{60};
        // How long the sensor must hold a new reading before Idle acts on it.
        std::chrono::seconds SensorDebounce{20};
    };

    struct CleanTankSettings
    {
        // How long the sensor must hold a new reading before it's acted on.
        std::chrono::seconds SensorDebounce{20};
        // How long the fill relay stays on each time the tank is filled.
        std::chrono::seconds FillRunTime{120};
        // How long the dosing pump runs to add detergent each time the tank is filled.
        std::chrono::seconds DetergentRunTime{9};
    };

    // Polling the robot and dock through the MovaHome/DreameHome cloud account.
    struct DreameSettings
    {
        bool Enabled = false;
        // "mova" for a MovaHome account, "dreame" for DreameHome.
        std::string Account = "mova";
        std::string Username;
        std::string Password;
        // Server region as chosen in the app, e.g. "us", "eu", "cn".
        std::string Country = "us";
        // Device ID or name; empty uses the first robot vacuum on the account.
        std::string Device;
        std::chrono::seconds PollInterval{30};
        // Subscribe to the app's MQTT push updates; polling slows down while they arrive.
        bool Push = true;
    };

    struct Settings
    {
        LogSettings Log;
        GpioSettings Gpio;
        DirtyTankSettings DirtyTank;
        CleanTankSettings CleanTank;
        DreameSettings Dreame;
    };

    // Where this platform looks for its config file; empty if it has none.
    std::string DefaultPath();

    // Loads settings from path (DefaultPath() when empty). A missing file gives the defaults
    // unless mustExist is set. Throws std::runtime_error naming the file and problem.
    Settings Load(const std::string& path, bool mustExist);
}
