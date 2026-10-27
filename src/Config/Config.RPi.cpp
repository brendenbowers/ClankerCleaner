// Raspberry Pi settings: read from a TOML file with toml++. Keys mirror
// config/clanker-cleaner.example.toml; missing keys keep the defaults in Config.h.

#include "Config/Config.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ClankerCleaner::Config
{
    namespace
    {
        // Header GPIOs on a Pi are lines 0 to 27.
        constexpr std::int64_t MaxPinLine = 27;
        constexpr std::int64_t MaxPumpRunSeconds = 60 * 60;
        constexpr std::int64_t MaxSensorDebounceSeconds = 10 * 60;
        // Kept short: the rinse fills the dirty tank from the house supply.
        constexpr std::int64_t MaxRinseSeconds = 5 * 60;
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

            // Typed reads for READ_SETTING: each assigns to target, which also supplies the
            // fallback unless one is given. Integers and durations need a minimum and maximum.
            void Read(std::string_view keyPath, std::string& target) const { target = ReadString(keyPath, target); }
            void Read(std::string_view keyPath, std::string& target, const std::string& fallback) const { target = ReadString(keyPath, fallback); }

            void Read(std::string_view keyPath, bool& target) const { target = ReadBool(keyPath, target); }
            void Read(std::string_view keyPath, bool& target, bool fallback) const { target = ReadBool(keyPath, fallback); }

            template<std::integral T>
                requires(!std::same_as<T, bool>)
            void Read(std::string_view keyPath, T& target, std::int64_t minimum, std::int64_t maximum) const
            {
                Read(keyPath, target, static_cast<std::int64_t>(target), minimum, maximum);
            }
            template<std::integral T>
                requires(!std::same_as<T, bool>)
            void Read(std::string_view keyPath, T& target, std::int64_t fallback, std::int64_t minimum, std::int64_t maximum) const
            {
                target = static_cast<T>(ReadInteger(keyPath, fallback, minimum, maximum));
            }

            // Durations are read as a count of their own unit, e.g. seconds for std::chrono::seconds.
            template<typename Rep, typename Period>
            void Read(std::string_view keyPath, std::chrono::duration<Rep, Period>& target, std::int64_t minimum, std::int64_t maximum) const
            {
                Read(keyPath, target, static_cast<std::int64_t>(target.count()), minimum, maximum);
            }
            template<typename Rep, typename Period>
            void Read(std::string_view keyPath, std::chrono::duration<Rep, Period>& target, std::int64_t fallback, std::int64_t minimum, std::int64_t maximum) const
            {
                target = std::chrono::duration<Rep, Period>(ReadInteger(keyPath, fallback, minimum, maximum));
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

        // "Gpio.DirtyTankSensorPin" -> "gpio.dirty_tank_sensor_pin".
        std::string KeyFromPath(std::string_view path)
        {
            std::string key;
            for(std::size_t index = 0; index < path.size(); ++index)
            {
                const char character = path[index];
                if(character >= 'A' && character <= 'Z')
                {
                    if(index > 0 && path[index - 1] != '.')
                    {
                        key += '_';
                    }
                    key += static_cast<char>(character - 'A' + 'a');
                }
                else
                {
                    key += character;
                }
            }
            return key;
        }
    }

// Reads settings.<path> with the Read overload for its type, from the key KeyFromPath gives.
// Extra arguments go to that overload: [fallback,] and for numbers minimum, maximum.
#define READ_SETTING(path, ...) reader.Read(KeyFromPath(#path), settings.path __VA_OPT__(, ) __VA_ARGS__)
// As READ_SETTING, for settings whose TOML key doesn't follow from the path.
#define READ_SETTING_KEY(path, key, ...) reader.Read(key, settings.path __VA_OPT__(, ) __VA_ARGS__)

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

        READ_SETTING_KEY(Log.Level, "logging.level");
        if(std::find(LogLevels.begin(), LogLevels.end(), settings.Log.Level) == LogLevels.end())
        {
            throw reader.Error(*table.at_path("logging.level").node(), "logging.level",
                               "expected trace, debug, info, warn, err, critical or off");
        }
        READ_SETTING_KEY(Log.FilePath, "logging.file");

        READ_SETTING(Gpio.DirtyTankSensorPin, 0, MaxPinLine);
        READ_SETTING(Gpio.CleanTankHighSensorPin, 0, MaxPinLine);
        READ_SETTING(Gpio.PumpRelayPin, 0, MaxPinLine);
        READ_SETTING(Gpio.DirtyTankRinseRelayPin, 0, MaxPinLine);
        READ_SETTING(Gpio.CleanTankFillRelayPin, 0, MaxPinLine);
        READ_SETTING(Gpio.CleanerDosingRelayPin, 0, MaxPinLine);
        READ_SETTING(Gpio.RelayActiveLow);

        const std::array<std::pair<const char*, std::uint32_t>, 6> assignedPins{{
            {"gpio.dirty_tank_sensor_pin", settings.Gpio.DirtyTankSensorPin},
            {"gpio.clean_tank_high_sensor_pin", settings.Gpio.CleanTankHighSensorPin},
            {"gpio.pump_relay_pin", settings.Gpio.PumpRelayPin},
            {"gpio.dirty_tank_rinse_relay_pin", settings.Gpio.DirtyTankRinseRelayPin},
            {"gpio.clean_tank_fill_relay_pin", settings.Gpio.CleanTankFillRelayPin},
            {"gpio.cleaner_dosing_relay_pin", settings.Gpio.CleanerDosingRelayPin},
        }};
        for(std::size_t firstIndex = 0; firstIndex < assignedPins.size(); ++firstIndex)
        {
            for(std::size_t secondIndex = firstIndex + 1; secondIndex < assignedPins.size(); ++secondIndex)
            {
                if(assignedPins[firstIndex].second == assignedPins[secondIndex].second)
                {
                    throw std::runtime_error(std::format("{}: {} and {} are both {}", filePath, assignedPins[firstIndex].first,
                                                         assignedPins[secondIndex].first, assignedPins[firstIndex].second));
                }
            }
        }

        READ_SETTING_KEY(DirtyTank.PumpRunTime, "dirty_tank.pump_run_seconds", 1, MaxPumpRunSeconds);
        READ_SETTING_KEY(DirtyTank.RinseTime, "dirty_tank.rinse_seconds", 1, MaxRinseSeconds);
        READ_SETTING_KEY(DirtyTank.RinsePumpRunTime, "dirty_tank.rinse_pump_run_seconds", 1, MaxPumpRunSeconds);
        READ_SETTING_KEY(DirtyTank.SensorDebounce, "dirty_tank.sensor_debounce_seconds", 0, MaxSensorDebounceSeconds);

        DreameSettings& dreame = settings.Dreame;
        READ_SETTING(Dreame.Enabled);
        READ_SETTING(Dreame.Account);
        if(dreame.Account != "mova" && dreame.Account != "dreame")
        {
            throw reader.Error(*table.at_path("dreame.account").node(), "dreame.account", "expected mova or dreame");
        }
        READ_SETTING(Dreame.Username);
        READ_SETTING(Dreame.Password);
        READ_SETTING(Dreame.Country);
        const bool countryIsCode = dreame.Country.size() >= 2 && dreame.Country.size() <= 3 &&
            std::all_of(dreame.Country.begin(), dreame.Country.end(), [](char character) { return character >= 'a' && character <= 'z'; });
        if(!countryIsCode)
        {
            throw std::runtime_error(std::format("{}: dreame.country: expected a lower-case region code such as \"us\" or \"eu\"", filePath));
        }
        READ_SETTING(Dreame.Device);
        READ_SETTING_KEY(Dreame.PollInterval, "dreame.poll_seconds", MinPollSeconds, MaxPollSeconds);
        READ_SETTING(Dreame.Push);
        if(dreame.Enabled && (dreame.Username.empty() || dreame.Password.empty()))
        {
            throw std::runtime_error(std::format("{}: dreame.enabled is true but dreame.username or dreame.password is empty", filePath));
        }

        return settings;
    }
}

#undef READ_SETTING
#undef READ_SETTING_KEY
