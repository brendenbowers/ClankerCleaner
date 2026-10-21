#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

namespace ClankerCleaner::Logging
{
    struct Options
    {
        // trace, debug, info, warn, err, critical or off.
        std::string Level = "info";
        // Rotating log file alongside the console; empty for console only.
        std::string FilePath;
        std::size_t MaxFileBytes = 5 * 1024 * 1024;
        std::size_t MaxFiles = 3;
    };

    // Sets up spdlog with a console sink (journald picks it up under systemd) and an optional
    // rotating file. The SPDLOG_LEVEL environment variable overrides Options::Level, including
    // per logger, e.g. SPDLOG_LEVEL=info,DirtyTank=debug. Throws if the level is unknown.
    void Initialize(const Options& options);

    // Named logger sharing the configured sinks, created on first use:
    //   Logging::Get("DirtyTank")->info("pump off after {} s", seconds);
    std::shared_ptr<spdlog::logger> Get(const std::string& name);

    // Flushes and releases the loggers; call before exit.
    void Shutdown();
}
