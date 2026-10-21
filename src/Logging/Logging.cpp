#include "Logging/Logging.h"

#include <mutex>
#include <stdexcept>
#include <vector>

#include <spdlog/cfg/env.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace ClankerCleaner::Logging
{
    namespace
    {
        std::mutex loggersMutex;
        std::vector<spdlog::sink_ptr> sharedSinks;
    }

    void Initialize(const Options& options)
    {
        const spdlog::level::level_enum level = spdlog::level::from_str(options.Level);
        if (level == spdlog::level::off && options.Level != "off")
        {
            throw std::invalid_argument("unknown log level '" + options.Level + "'");
        }

        std::lock_guard<std::mutex> lock(loggersMutex);
        sharedSinks.clear();
        sharedSinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        if (!options.FilePath.empty())
        {
            sharedSinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                options.FilePath, options.MaxFileBytes, options.MaxFiles));
        }

        spdlog::set_default_logger(std::make_shared<spdlog::logger>("Main", sharedSinks.begin(), sharedSinks.end()));
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e [%^%l%$] %n: %v");
        spdlog::set_level(level);
        spdlog::cfg::load_env_levels();
        // Low volume, so flush every line; nothing is lost if the process is killed.
        spdlog::flush_on(spdlog::level::trace);
    }

    std::shared_ptr<spdlog::logger> Get(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(loggersMutex);
        if (auto existing = spdlog::get(name))
        {
            return existing;
        }

        auto logger = std::make_shared<spdlog::logger>(name, sharedSinks.begin(), sharedSinks.end());
        // Registers it and applies the global pattern, level and any SPDLOG_LEVEL entry for this name.
        spdlog::initialize_logger(logger);
        return logger;
    }

    void Shutdown()
    {
        spdlog::shutdown();
    }
}
