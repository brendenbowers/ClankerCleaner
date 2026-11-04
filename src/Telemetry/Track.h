#pragma once

// TELEMETRY_TRACK(Event, args...) reports something that happened, e.g.
// TELEMETRY_TRACK(OutputChanged, pin, active). With MQTT built in (the CLANKER_MQTT CMake
// option) it calls the Telemetry function named Event, which publishes it. Built without,
// it's a no-op and the arguments aren't evaluated, so they mustn't have side effects.
//
// Topics, under the [mqtt] topic prefix. Retained ones hold the latest state; the others
// are one message per event.
//   sensors/<name>       {"wet":true,"level":"low"}                     retained, on each change
//   outputs/<name>       {"on":true}                                    retained, on each change
//   outputs/<name>/runs  {"seconds":92.4}                               each time it turns off
//   flows/<flow>/state   {"state":"DIRTY_TANK_EMPTY","phase":"begin"}   retained, on entering a
//                        state, or "phase":"abort" with "error" when one aborts
//   flows/<flow>/runs    {"state":"...","seconds":92.5,"result":"done"} as each state ends
//                        ("aborted", with "error", if it aborted)
//   dock                 the robot and dock status from the cloud       retained, on each change
#if CLANKER_MQTT

#include <cstdint>
#include <string>

namespace ClankerCleaner::Config
{
    struct Settings;
}
namespace ClankerCleaner::Dreame
{
    struct DockStatus;
}
namespace ClankerCleaner::GPIO
{
    struct PinState;
}
namespace ClankerCleaner::StateMachine
{
    enum class StatePhase;
}

namespace ClankerCleaner::Telemetry
{
    // Connects to the broker if [mqtt] is enabled; events do nothing otherwise. Call Start
    // before anything is tracked, and Stop once nothing can be.
    void Start(const Config::Settings& settings);
    void Stop();

    // On the ESP32 this runs on the default event loop task, which has a small stack.
    void SensorChanged(std::uint32_t pin, const GPIO::PinState& state);
    void OutputChanged(std::uint32_t pin, bool active);
    void FlowPhaseChanged(const std::string& flow, const std::string& state, StateMachine::StatePhase phase, const std::string& error);
    void DockChanged(const Dreame::DockStatus& status);
}

#define TELEMETRY_TRACK(event, ...) ::ClankerCleaner::Telemetry::event(__VA_ARGS__)

#else

#define TELEMETRY_TRACK(event, ...) static_cast<void>(0)

#endif
