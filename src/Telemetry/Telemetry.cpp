#include "Telemetry/Track.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>

#include <nlohmann/json.hpp>

#include "Config/Config.h"
#include "Dreame/Types.h"
#include "GPIO/Pin.h"
#include "Logging/Logging.h"
#include "Net/MqttPublisher.h"
#include "StateMachine/State.h"

namespace ClankerCleaner::Telemetry
{
    namespace
    {
        using json = nlohmann::json;
        using Clock = std::chrono::steady_clock;

        struct Sensor
        {
            std::string Name;
            Config::SensorOutput Output = Config::SensorOutput::Npn;
        };

        struct Tracker
        {
            explicit Tracker(const Config::MqttSettings& settings)
                : Publisher(settings)
            {
            }

            Net::MqttPublisher Publisher;
            std::map<std::uint32_t, Sensor> Sensors;
            std::map<std::uint32_t, std::string> Outputs;
            // When each output turned on, for its run time when it turns off. Outputs change
            // on the machine thread, and all turn off from the main thread at shutdown.
            std::mutex OutputsMutex;
            std::map<std::uint32_t, Clock::time_point> OnSince;
            // When each flow entered its state. Machine thread only.
            std::map<std::string, Clock::time_point> EnteredAt;
        };

        // Set before anything is tracked and cleared after, so the events don't lock to read it.
        std::unique_ptr<Tracker> tracker;

        double SecondsSince(Clock::time_point start)
        {
            const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
            return std::round(seconds * 10) / 10;
        }

        // Built by hand rather than with nlohmann: on the ESP32 pin edges arrive on the
        // event loop task, which has a 4 KB stack.
        std::string SensorPayload(bool wet, bool high)
        {
            return std::string("{\"wet\":") + (wet ? "true" : "false") + ",\"level\":\"" + (high ? "high" : "low") + "\"}";
        }
    }

    void Start(const Config::Settings& settings)
    {
        if (!settings.Mqtt.Enabled)
        {
            Logging::Get("Mqtt")->info("publishing off ([mqtt] enabled = false)");
            return;
        }

        const Config::GpioSettings& gpio = settings.Gpio;
        tracker = std::make_unique<Tracker>(settings.Mqtt);
        tracker->Sensors = {
            {gpio.DirtyTankSensorPin, {"dirty_tank", gpio.DirtyTankSensorOutput}},
            {gpio.CleanTankHighSensorPin, {"clean_tank_high", gpio.CleanTankHighSensorOutput}},
            {gpio.CleanTankLowSensorPin, {"clean_tank_low", gpio.CleanTankLowSensorOutput}},
        };
        tracker->Outputs = {
            {gpio.PumpRelayPin, "pump"},
            {gpio.DirtyTankRinseRelayPin, "dirty_tank_rinse"},
            {gpio.CleanTankFillRelayPin, "clean_tank_fill"},
            {gpio.CleanerDosingRelayPin, "cleaner_dosing"},
        };
        // Outputs start off; this replaces whatever was retained from before a restart.
        for (const auto& [pin, name] : tracker->Outputs)
        {
            tracker->Publisher.Publish("outputs/" + name, "{\"on\":false}", true);
        }
        tracker->Publisher.Start();
    }

    void Stop()
    {
        // The publisher sends what's queued before it stops.
        tracker.reset();
    }

    void SensorChanged(std::uint32_t pin, const GPIO::PinState& state)
    {
        if (!tracker || state == GPIO::PinState::Default)
        {
            return;
        }
        const auto sensor = tracker->Sensors.find(pin);
        if (sensor == tracker->Sensors.end())
        {
            return;
        }
        // Triggered means wet: an NPN sensor pulls the line LOW, a PNP one drives it HIGH.
        const bool high = (sensor->second.Output == Config::SensorOutput::Pnp) == state.Triggered;
        tracker->Publisher.Publish("sensors/" + sensor->second.Name, SensorPayload(state.Triggered, high), true);
    }

    void OutputChanged(std::uint32_t pin, bool active)
    {
        if (!tracker)
        {
            return;
        }
        const auto name = tracker->Outputs.find(pin);
        if (name == tracker->Outputs.end())
        {
            return;
        }
        const std::string topic = "outputs/" + name->second;
        tracker->Publisher.Publish(topic, active ? "{\"on\":true}" : "{\"on\":false}", true);

        std::lock_guard<std::mutex> lock(tracker->OutputsMutex);
        if (active)
        {
            tracker->OnSince[pin] = Clock::now();
            return;
        }
        const auto onSince = tracker->OnSince.find(pin);
        if (onSince != tracker->OnSince.end())
        {
            char seconds[32];
            std::snprintf(seconds, sizeof(seconds), "%.1f", SecondsSince(onSince->second));
            tracker->OnSince.erase(onSince);
            tracker->Publisher.Publish(topic + "/runs", std::string("{\"seconds\":") + seconds + "}");
        }
    }

    void FlowPhaseChanged(const std::string& flow, const std::string& state, StateMachine::StatePhase phase, const std::string& error)
    {
        using StateMachine::StatePhase;

        if (!tracker)
        {
            return;
        }
        const std::string topic = "flows/" + flow;
        if (phase == StatePhase::Begin)
        {
            tracker->EnteredAt[flow] = Clock::now();
            tracker->Publisher.Publish(topic + "/state", json{{"state", state}, {"phase", "begin"}}.dump(), true);
            return;
        }
        if (phase != StatePhase::End && phase != StatePhase::Abort)
        {
            return;
        }

        const bool aborted = phase == StatePhase::Abort;
        json run{{"state", state}, {"seconds", SecondsSince(tracker->EnteredAt[flow])}, {"result", aborted ? "aborted" : "done"}};
        if (aborted)
        {
            json abortedState{{"state", state}, {"phase", "abort"}};
            if (!error.empty())
            {
                run["error"] = error;
                abortedState["error"] = error;
            }
            tracker->Publisher.Publish(topic + "/state", abortedState.dump(), true);
        }
        tracker->Publisher.Publish(topic + "/runs", run.dump());
    }

    void DockChanged(const Dreame::DockStatus& status)
    {
        if (!tracker)
        {
            return;
        }
        const json payload{
            {"online", status.Online},
            {"robot", Dreame::ToString(status.Robot)},
            {"status", Dreame::ToString(status.Status)},
            {"task", Dreame::ToString(status.Task)},
            {"base", Dreame::ToString(status.Base)},
            {"clean_tank", Dreame::ToString(status.CleanTank)},
            {"dirty_tank", Dreame::ToString(status.DirtyTank)},
            {"low_water", Dreame::ToString(status.LowWater)},
            {"detergent", Dreame::ToString(status.Detergent)},
            {"error_code", status.ErrorCode},
            {"cleaning", status.IsRobotCleaning()},
            {"washing_mop", status.IsWashingMop()},
            {"clean_tank_empty", status.IsCleanTankEmpty()},
            {"dirty_tank_full", status.IsDirtyTankFull()},
        };
        tracker->Publisher.Publish("dock", payload.dump(), true);
    }
}
