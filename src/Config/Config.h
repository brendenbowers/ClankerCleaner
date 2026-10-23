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

    struct GpioSettings
    {
        // Line (BCM GPIO) numbers, not physical header pins.
        std::uint32_t DirtyTankSensorPin = GPIO::Constants::HIGH_WASTE_WATER_TANK;
        std::uint32_t PumpRelayPin = GPIO::Constants::PUMP_RELAY_PIN;
        // True for a relay board with its trigger jumper on L.
        bool RelayActiveLow = false;
    };

    struct DirtyTankSettings
    {
        std::chrono::seconds PumpRunTime{240};
        // How long the sensor must hold a new reading before Idle acts on it.
        std::chrono::seconds SensorDebounce{20};
    };

    struct Settings
    {
        LogSettings Log;
        GpioSettings Gpio;
        DirtyTankSettings DirtyTank;
    };

    // Where this platform looks for its config file; empty if it has none.
    std::string DefaultPath();

    // Loads settings from path (DefaultPath() when empty). A missing file gives the defaults
    // unless mustExist is set. Throws std::runtime_error naming the file and problem.
    Settings Load(const std::string& path, bool mustExist);
}
