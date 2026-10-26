// Raspberry Pi settings: read from a TOML file with toml++. Keys mirror
// config/clanker-cleaner.example.toml; missing keys keep the defaults in Config.h.

#include "Config/Config.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace ClankerCleaner::Config
{
    namespace
    {
        // Header GPIOs on a Pi are lines 0 to 27.
        constexpr std::int64_t MaxPinLine = 27;
        constexpr std::int64_t MaxPumpRunSeconds = 60 * 60;
        constexpr std::int64_t MaxSensorDebounceSeconds = 10 * 60;
        // Faster than every 10 s risks the cloud rate-limiting the account.
        constexpr std::int64_t MinPollSeconds = 10;
        constexpr std::int64_t MaxPollSeconds = 60 * 60;
        constexpr std::array LogLevels{"trace", "debug", "info", "warn", "err", "critical", "off"};

        class SettingsReader
        {
        public:
            SettingsReader(const toml::table& table, const std::string& filePath)
                : table(table)
                , filePath(filePath)
            {
            }

            std::string ReadString(std::string_view keyPath, const std::string& fallback) const
            {
                const toml::node* node = Find(keyPath);
                if(!node)
                {
                    return fallback;
                }
                if(auto value = node->value<std::string>())
                {
                    return *value;
                }
                throw Error(*node, keyPath, "expected a string");
            }

            bool ReadBool(std::string_view keyPath, bool fallback) const
            {
                const toml::node* node = Find(keyPath);
                if(!node)
                {
                    return fallback;
                }
                if(auto value = node->value_exact<bool>())
                {
                    return *value;
                }
                throw Error(*node, keyPath, "expected true or false");
            }

            std::int64_t ReadInteger(std::string_view keyPath, std::int64_t fallback, std::int64_t minimum, std::int64_t maximum) const
            {
                const toml::node* node = Find(keyPath);
                if(!node)
                {
                    return fallback;
                }
                auto value = node->value_exact<std::int64_t>();
                if(!value)
                {
                    throw Error(*node, keyPath, "expected a whole number");
                }
                if(*value < minimum || *value > maximum)
                {
                    throw Error(*node, keyPath, std::format("{} is outside {} to {}", *value, minimum, maximum));
                }
                return *value;
            }

            std::runtime_error Error(const toml::node& node, std::string_view keyPath, std::string_view problem) const
            {
                const toml::source_position position = node.source().begin;
                return std::runtime_error(std::format("{}:{}:{}: {}: {}", filePath, position.line, position.column, keyPath, problem));
            }

        private:
            const toml::node* Find(std::string_view keyPath) const
            {
                return table.at_path(keyPath).node();
            }

            const toml::table& table;
            const std::string& filePath;
        };
    }

    std::string DefaultPath()
    {
        return "/etc/clanker-cleaner/config.toml";
    }

    Settings Load(const std::string& path, bool mustExist)
    {
        Settings settings;
        const std::string filePath = path.empty() ? DefaultPath() : path;

        if(!std::filesystem::exists(filePath))
        {
            if(mustExist)
            {
                throw std::runtime_error("config file not found: " + filePath);
            }
            return settings;
        }

        toml::table table;
        try
        {
            table = toml::parse_file(filePath);
        }
        catch(const toml::parse_error& error)
        {
            const toml::source_position position = error.source().begin;
            throw std::runtime_error(std::format("{}:{}:{}: {}", filePath, position.line, position.column, error.description()));
        }

        const SettingsReader reader(table, filePath);

        settings.Log.Level = reader.ReadString("logging.level", settings.Log.Level);
        if(std::find(LogLevels.begin(), LogLevels.end(), settings.Log.Level) == LogLevels.end())
        {
            throw reader.Error(*table.at_path("logging.level").node(), "logging.level",
                               "expected trace, debug, info, warn, err, critical or off");
        }
        settings.Log.FilePath = reader.ReadString("logging.file", settings.Log.FilePath);

        settings.Gpio.DirtyTankSensorPin = static_cast<std::uint32_t>(
            reader.ReadInteger("gpio.dirty_tank_sensor_pin", settings.Gpio.DirtyTankSensorPin, 0, MaxPinLine));
        settings.Gpio.PumpRelayPin = static_cast<std::uint32_t>(
            reader.ReadInteger("gpio.pump_relay_pin", settings.Gpio.PumpRelayPin, 0, MaxPinLine));
        settings.Gpio.DirtyTankRinseRelayPin = static_cast<std::uint32_t>(
            reader.ReadInteger("gpio.dirty_tank_rinse_relay_pin", settings.Gpio.DirtyTankRinseRelayPin, 0, MaxPinLine));
        settings.Gpio.RelayActiveLow = reader.ReadBool("gpio.relay_active_low", settings.Gpio.RelayActiveLow);
        if(settings.Gpio.DirtyTankSensorPin == settings.Gpio.PumpRelayPin)
        {
            throw std::runtime_error(std::format("{}: gpio.dirty_tank_sensor_pin and gpio.pump_relay_pin are both {}",
                                                 filePath, settings.Gpio.PumpRelayPin));
        }

        settings.DirtyTank.PumpRunTime = std::chrono::seconds(
            reader.ReadInteger("dirty_tank.pump_run_seconds", settings.DirtyTank.PumpRunTime.count(), 1, MaxPumpRunSeconds));
        settings.DirtyTank.SensorDebounce = std::chrono::seconds(
            reader.ReadInteger("dirty_tank.sensor_debounce_seconds", settings.DirtyTank.SensorDebounce.count(), 0, MaxSensorDebounceSeconds));

        DreameSettings& dreame = settings.Dreame;
        dreame.Enabled = reader.ReadBool("dreame.enabled", dreame.Enabled);
        dreame.Account = reader.ReadString("dreame.account", dreame.Account);
        if(dreame.Account != "mova" && dreame.Account != "dreame")
        {
            throw reader.Error(*table.at_path("dreame.account").node(), "dreame.account", "expected mova or dreame");
        }
        dreame.Username = reader.ReadString("dreame.username", dreame.Username);
        dreame.Password = reader.ReadString("dreame.password", dreame.Password);
        dreame.Country = reader.ReadString("dreame.country", dreame.Country);
        const bool countryIsCode = dreame.Country.size() >= 2 && dreame.Country.size() <= 3 &&
            std::all_of(dreame.Country.begin(), dreame.Country.end(), [](char character) { return character >= 'a' && character <= 'z'; });
        if(!countryIsCode)
        {
            throw std::runtime_error(std::format("{}: dreame.country: expected a lower-case region code such as \"us\" or \"eu\"", filePath));
        }
        dreame.Device = reader.ReadString("dreame.device", dreame.Device);
        dreame.PollInterval = std::chrono::seconds(
            reader.ReadInteger("dreame.poll_seconds", dreame.PollInterval.count(), MinPollSeconds, MaxPollSeconds));
        dreame.Push = reader.ReadBool("dreame.push", dreame.Push);
        if(dreame.Enabled && (dreame.Username.empty() || dreame.Password.empty()))
        {
            throw std::runtime_error(std::format("{}: dreame.enabled is true but dreame.username or dreame.password is empty", filePath));
        }

        return settings;
    }
}
