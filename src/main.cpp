#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <cstdint>

#include <memory>
#include <optional>

#include <argparse/argparse.hpp>

#include "Config/Config.h"
#include "Dreame/DockMonitor.h"

#include "GPIO/PinInterface.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"
#include "Logging/Logging.h"
#include "StateMachine/Machine.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/States/EmptyTank.h"
#include "Flows/DirtyTank/States/Rinse.h"

namespace
{
    // Global atomic flag to safely communicate with the signal handler
    std::atomic<bool> run{true};

    // Signal handlers must use C linkage
    extern "C" void signal_handler(int signal) {
        if (signal == SIGINT || signal == SIGTERM) {
            run = false; // Safe to do inside a signal handler
        }
    }

    // Values given on the command line; anything set here overrides the config file.
    struct CommandLine
    {
        std::string ConfigPath;
        std::optional<std::string> LogLevel;
        std::optional<std::string> LogFile;
    };

    // --help and --version are added by argparse; both print and exit.
    bool ParseArguments(int argumentCount, char** arguments, CommandLine& commandLine)
    {
        argparse::ArgumentParser program("clanker_cleaner", CLANKER_VERSION);
        program.add_description("Auto-drain and fill controller for the MOVA P10 Ultra dock.");
        program.add_epilog("Command-line options override the config file. SPDLOG_LEVEL overrides both,\n"
                           "per logger too, e.g. SPDLOG_LEVEL=info,DirtyTank=debug");

        program.add_argument("--config")
            .help("settings file (default " + ClankerCleaner::Config::DefaultPath() + ", optional)")
            .metavar("PATH");

        program.add_argument("--log-level")
            .help("minimum level to log (default info)")
            .choices("trace", "debug", "info", "warn", "err", "critical", "off")
            .nargs(1)
            .metavar("LEVEL");

        program.add_argument("--log-file")
            .help("also write to this rotating log file")
            .metavar("PATH");

        try
        {
            program.parse_args(argumentCount, arguments);
        }
        catch (const std::exception& error)
        {
            std::cerr << error.what() << "\n\n" << program;
            return false;
        }

        commandLine.ConfigPath = program.present("--config").value_or("");
        commandLine.LogLevel = program.present("--log-level");
        commandLine.LogFile = program.present("--log-file");
        return true;
    }

    std::string_view PhaseName(ClankerCleaner::StateMachine::StatePhase phase)
    {
        using ClankerCleaner::StateMachine::StatePhase;
        switch (phase)
        {
        case StatePhase::None: return "none";
        case StatePhase::Begin: return "begin";
        case StatePhase::Running: return "running";
        case StatePhase::End: return "end";
        case StatePhase::Abort: return "abort";
        }
        return "unknown";
    }

    // Logs every phase change to the flow's own logger: entering a state at info,
    // aborts at warn with the reason, everything else at debug.
    void LogPhases(ClankerCleaner::StateMachine::Flow& flow)
    {
        using ClankerCleaner::StateMachine::Flow;
        using ClankerCleaner::StateMachine::State;
        using ClankerCleaner::StateMachine::StatePhase;

        auto flowLogger = ClankerCleaner::Logging::Get(flow.GetName());
        flow.SetPhaseListener([flowLogger](const Flow& changedFlow, const State& state, StatePhase phase)
        {
            if (phase == StatePhase::Abort)
            {
                const std::string reason = changedFlow.GetLastError();
                flowLogger->warn("{} aborted{}{}", state.GetName(), reason.empty() ? "" : ": ", reason);
            }
            else if (phase == StatePhase::Begin)
            {
                flowLogger->info("entering {}", state.GetName());
            }
            else
            {
                flowLogger->debug("{} {}", state.GetName(), PhaseName(phase));
            }
        });
    }

    // Logs the new state on every change, plus a line of its own when the things the
    // flows care about flip.
    void LogDockChange(const ClankerCleaner::Dreame::DockStatus& previous, const ClankerCleaner::Dreame::DockStatus& current)
    {
        auto dockLogger = ClankerCleaner::Logging::Get("Dock");
        dockLogger->info("{}", ClankerCleaner::Dreame::Describe(current));
        if (!current.Online)
        {
            return;
        }
        if (current.IsDirtyTankFull() != previous.IsDirtyTankFull())
        {
            dockLogger->warn("{}", current.IsDirtyTankFull() ? "dock reports the dirty tank full (or removed)" : "dock reports the dirty tank ok");
        }
        if (current.IsCleanTankEmpty() != previous.IsCleanTankEmpty())
        {
            dockLogger->warn("{}", current.IsCleanTankEmpty() ? "dock reports the clean tank empty" : "dock reports the clean tank ok");
        }
        if (current.IsRobotCleaning() != previous.IsRobotCleaning())
        {
            dockLogger->info("{}", current.IsRobotCleaning() ? "robot started cleaning" : "robot stopped cleaning");
        }
    }

    std::unique_ptr<ClankerCleaner::Dreame::DockMonitor> CreateDockMonitor(const ClankerCleaner::Config::DreameSettings& settings,
                                                                           ClankerCleaner::StateMachine::Machine& machine)
    {
        namespace Dreame = ClankerCleaner::Dreame;

        if (!settings.Enabled)
        {
            ClankerCleaner::Logging::Get("Dock")->info("cloud polling off ([dreame] enabled = false)");
            return nullptr;
        }

        Dreame::CloudOptions options;
        options.Account = settings.Account == "dreame" ? Dreame::AccountType::Dreame : Dreame::AccountType::Mova;
        options.Username = settings.Username;
        options.Password = settings.Password;
        options.Country = settings.Country;
        options.Device = settings.Device;

        auto monitor = std::make_unique<Dreame::DockMonitor>(std::move(options), settings.PollInterval, settings.Push);
        // Wake the flows so states see the new status straight away.
        monitor->AddChangeListener([&machine](const Dreame::DockStatus& previous, const Dreame::DockStatus& current)
        {
            LogDockChange(previous, current);
            machine.Wake();
        });
        return monitor;
    }
}


void ConfigurePins(ClankerCleaner::GPIO::PinInterface& pins, ClankerCleaner::Config::Settings& settings)
{
    namespace GPIO = ClankerCleaner::GPIO;
    namespace Logging = ClankerCleaner::Logging;

    auto gpioLog = Logging::Get("GPIO");

    pins.MonitorPin(settings.Gpio.DirtyTankSensorPin, GPIO::Edge::Falling);
    pins.AddOutputPin(settings.Gpio.PumpRelayPin, settings.Gpio.RelayActiveLow);
    pins.AddOutputPin(settings.Gpio.DirtyTankRinseRelayPin, settings.Gpio.RelayActiveLow);
    pins.AddOutputPin(settings.Gpio.CleanTankFillRelayPin, settings.Gpio.RelayActiveLow);
    pins.AddOutputPin(settings.Gpio.CleanerDosingRelayPin, settings.Gpio.RelayActiveLow);
    pins.Start();

    for (const auto& [id, pin] : pins.GetInputPins())
    {
        gpioLog->info("Monitoring Pin {}", id);
    }

    for (const auto& [id, pin] : pins.GetOutputPins())
    {
        gpioLog->info("Output Pin {}", id);
    }
}

void ConfigureFlows(ClankerCleaner::StateMachine::Machine& machine, ClankerCleaner::GPIO::PinInterface& pins, ClankerCleaner::Config::Settings& settings,
                    ClankerCleaner::Dreame::DockMonitor* dockMonitor)
{
    namespace StateMachine = ClankerCleaner::StateMachine;
    namespace DirtyTank = ClankerCleaner::Flows::DirtyTank;

    DirtyTank::DirtyTankFlow& dirtyTankFlow = machine.AddFlow<DirtyTank::DirtyTankFlow>(DirtyTank::FLOWNAME);
    dirtyTankFlow.pinMonitor = &pins;
    dirtyTankFlow.settings = &settings;
    dirtyTankFlow.dockMonitor = dockMonitor;
    dirtyTankFlow.AddState<DirtyTank::States::Idle>();
    dirtyTankFlow.AddState<DirtyTank::States::EmptyTank>();
    dirtyTankFlow.AddState<DirtyTank::States::Rinse>();
    dirtyTankFlow.SetLooping(true);
    LogPhases(dirtyTankFlow);
}

int main(int argumentCount, char** arguments)
{

    namespace GPIO = ClankerCleaner::GPIO;
    namespace StateMachine = ClankerCleaner::StateMachine;
    namespace Logging = ClankerCleaner::Logging;

    CommandLine commandLine;
    if (!ParseArguments(argumentCount, arguments, commandLine))
    {
        return EXIT_FAILURE;
    }

    // An explicit --config must exist; the default path is optional.
    namespace Config = ClankerCleaner::Config;
    Config::Settings settings;
    try
    {
        settings = Config::Load(commandLine.ConfigPath, !commandLine.ConfigPath.empty());
    }
    catch (const std::exception& error)
    {
        std::cerr << "Config error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    if (commandLine.LogLevel)
    {
        settings.Log.Level = *commandLine.LogLevel;
    }
    if (commandLine.LogFile)
    {
        settings.Log.FilePath = *commandLine.LogFile;
    }

    try
    {
        Logging::Options logOptions;
        logOptions.Level = settings.Log.Level;
        logOptions.FilePath = settings.Log.FilePath;
        Logging::Initialize(logOptions);
    }
    catch (const std::exception& error)
    {
        std::cerr << "Logging setup failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    auto log = Logging::Get("Main");
    int exitCode = EXIT_SUCCESS;
    log->info("ClankerCleaner {} starting on {}", CLANKER_VERSION, CLANKER_PLATFORM);
    log->info("config: {}, pump run time {} s, sensor debounce {} s",
              commandLine.ConfigPath.empty() ? Config::DefaultPath() + " (if present)" : commandLine.ConfigPath,
              settings.DirtyTank.PumpRunTime.count(), settings.DirtyTank.SensorDebounce.count());

    try
    {
        // Pins: the dirty tank sensor pulls LOW when wet, so a falling edge is the trigger.
        GPIO::PinInterface pins;
        ConfigurePins(pins, settings);

        // Flows: Idle waits for the sensor, EmptyTank runs the pump, then back to Idle.
        StateMachine::Machine machine;
        // Declared after the machine so it's destroyed (and stopped) first.
        std::unique_ptr<ClankerCleaner::Dreame::DockMonitor> dockMonitor = CreateDockMonitor(settings.Dreame, machine);
        ConfigureFlows(machine, pins, settings, dockMonitor.get());

        if (dockMonitor)
        {
            dockMonitor->Start();
        }
        machine.Start();
        machine.StartFlow(ClankerCleaner::Flows::DirtyTank::FLOWNAME);
        log->info("state machine started");

        // Run until a stop signal, or until every flow has finished or aborted.
        bool flowsEnded = false;
        while (run && !flowsEnded)
        {
            flowsEnded = machine.WaitUntilIdle(std::chrono::milliseconds(500));
        }

        if (flowsEnded)
        {
            for (const StateMachine::Flow* flow : machine.GetFlows())
            {
                if (flow->GetStatus() == StateMachine::FlowStatus::Aborted)
                {
                    log->error("flow {} aborted: {}", flow->GetName(), flow->GetLastError());
                    exitCode = EXIT_FAILURE;
                }
            }
            log->info("all flows ended");
        }
        else
        {
            log->info("stop requested");
        }

        log->info("shutting down");
        if (dockMonitor)
        {
            dockMonitor->Stop();
        }
        // Stopping the machine aborts running flows, so states turn their outputs off first.
        machine.Stop();
        pins.StopAndWait();
        log->info("stopped");
    }
    catch (const std::exception& error)
    {
        log->critical("fatal: {}", error.what());
        exitCode = EXIT_FAILURE;
    }

    Logging::Shutdown();
    return exitCode;
}