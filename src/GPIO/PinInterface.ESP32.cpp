// ESP32 half of PinInterface (ESP-IDF GPIO driver): configuring pins, edge handling and
// driving outputs. The shared half is in PinInterface.cpp.
//
// Each input's interrupt fires on both edges and posts the pin and time to ESP-IDF's default
// event loop; its task (the one WiFi's events also run on) then reads the pin's level, so an
// edge that bounced straight back is dropped, and hands it to OnEdge. OnEdge locks a mutex,
// which an interrupt can't.

#include "GPIO/PinInterface.h"
#include "GPIO/Platform.ESP32.h"

#include <deque>
#include <map>
#include <stdexcept>
#include <string>

#include <esp_attr.h>
#include <esp_err.h>
#include <esp_event.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

namespace ClankerCleaner::GPIO
{
    namespace
    {
        // Event ids are GPIO numbers.
        ESP_EVENT_DEFINE_BASE(ClankerGpioEvent);

        // Handed to the interrupt handler, so it must stay put while the handler is added.
        struct InterruptContext
        {
            gpio_num_t Pin;
        };

        void IRAM_ATTR OnInterrupt(void* argument)
        {
            const auto* context = static_cast<const InterruptContext*>(argument);
            // An ISR event carries at most 4 bytes: the low half of the esp_timer time (the
            // same clock as std::chrono::steady_clock), widened again by the handler.
            const std::uint32_t timeUs = static_cast<std::uint32_t>(esp_timer_get_time());
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            // A full event queue drops the edge, as the old edge queue did; the next one corrects the level.
            esp_event_isr_post(ClankerGpioEvent, context->Pin, &timeUs, sizeof(timeUs), &higherPriorityTaskWoken);
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }

        void Check(esp_err_t result, const char* action, gpio_num_t pin)
        {
            if(result != ESP_OK)
            {
                throw std::runtime_error(std::string(action) + " GPIO" + std::to_string(pin) + " failed: " + esp_err_to_name(result));
            }
        }

        Edge EdgeForLevel(int level)
        {
            return level ? Edge::Rising : Edge::Falling;
        }
    }

    struct PinInterface::PlatformState
    {
        // Runs on the event loop task. A member, so it can reach the private OnEdge.
        static void OnEdgeEvent(void* argument, esp_event_base_t /*base*/, int32_t id, void* data)
        {
            auto& pins = *static_cast<PinInterface*>(argument);
            PlatformState& state = *pins.platformState;
            const gpio_num_t pin = static_cast<gpio_num_t>(id);

            // Back to a full time: the interrupt was less than 2^32 us (71 minutes) ago.
            const std::int64_t nowUs = esp_timer_get_time();
            const std::uint32_t ageUs = static_cast<std::uint32_t>(nowUs) - *static_cast<const std::uint32_t*>(data);
            const std::int64_t timeUs = nowUs - ageUs;

            const int level = gpio_get_level(pin);
            int& lastLevel = state.Levels[pin];
            if(level == lastLevel)
            {
                return;
            }
            lastLevel = level;

            pins.OnEdge(pin, EdgeForLevel(level), static_cast<std::uint64_t>(timeUs) * 1000, ++state.Sequences[pin]);
        }

        std::deque<InterruptContext> Interrupts;
        esp_event_handler_instance_t EdgeHandler = nullptr;
        // Used only on the event loop task once the handler is registered.
        std::map<gpio_num_t, int> Levels;
        std::map<gpio_num_t, std::uint32_t> Sequences;
    };

    PinInterface::PinInterface()
        : platformState(std::make_unique<PlatformState>())
    {
    }

    PinInterface::~PinInterface()
    {
        StopAndWait();
    }

    void PinInterface::PlatformStart()
    {
        PlatformLineConfig config;
        for(const auto& pinEntry : outputPins)
        {
            pinEntry.second.Configure(config);
        }
        for(const auto& pinEntry : monitoredPins)
        {
            pinEntry.second.Configure(config);
        }

        for(const PlatformLineConfig::Line& line : config.Lines)
        {
            if(line.Config.mode == GPIO_MODE_OUTPUT)
            {
                Check(gpio_set_level(line.Pin, line.InitialLevel), "setting", line.Pin);
            }
            Check(gpio_config(&line.Config), "configuring", line.Pin);
        }

        if(monitoredPins.empty())
        {
            return;
        }

        // Network::Start normally creates it first; already created is fine.
        const esp_err_t loopResult = esp_event_loop_create_default();
        if(loopResult != ESP_OK && loopResult != ESP_ERR_INVALID_STATE)
        {
            throw std::runtime_error(std::string("event loop for GPIO edges failed: ") + esp_err_to_name(loopResult));
        }
        // Already installed is fine; it's shared with anything else using GPIO interrupts.
        const esp_err_t serviceResult = gpio_install_isr_service(0);
        if(serviceResult != ESP_OK && serviceResult != ESP_ERR_INVALID_STATE)
        {
            throw std::runtime_error(std::string("GPIO interrupt service failed: ") + esp_err_to_name(serviceResult));
        }

        // Start from the actual levels, like the Pi does.
        for(auto& [pinId, pin] : monitoredPins)
        {
            const gpio_num_t gpio = static_cast<gpio_num_t>(pinId);
            const int level = gpio_get_level(gpio);
            platformState->Levels[gpio] = level;
            pin.HandleEdge(EdgeForLevel(level), 0, 0);
        }

        const esp_err_t handlerResult = esp_event_handler_instance_register(ClankerGpioEvent, ESP_EVENT_ANY_ID, PlatformState::OnEdgeEvent,
                                                                            this, &platformState->EdgeHandler);
        if(handlerResult != ESP_OK)
        {
            throw std::runtime_error(std::string("GPIO edge handler failed: ") + esp_err_to_name(handlerResult));
        }

        for(const auto& pinEntry : monitoredPins)
        {
            const gpio_num_t gpio = static_cast<gpio_num_t>(pinEntry.first);
            InterruptContext& context = platformState->Interrupts.emplace_back(InterruptContext{gpio});
            Check(gpio_isr_handler_add(gpio, OnInterrupt, &context), "watching", gpio);
        }
    }

    void PinInterface::PlatformStop()
    {
        // No new events first; then unregistering waits for a handler call already running.
        for(const InterruptContext& context : platformState->Interrupts)
        {
            gpio_isr_handler_remove(context.Pin);
            gpio_reset_pin(context.Pin);
        }
        platformState->Interrupts.clear();

        if(platformState->EdgeHandler)
        {
            esp_event_handler_instance_unregister(ClankerGpioEvent, ESP_EVENT_ANY_ID, platformState->EdgeHandler);
            platformState->EdgeHandler = nullptr;
        }
        platformState->Levels.clear();
        platformState->Sequences.clear();

        // Outputs stay driven at their off level (AllOutputsOff ran first). Resetting them
        // would float the relay inputs, with the pin's default pull-up on.
    }

    void PinInterface::PlatformSetOutput(std::uint32_t pin, bool active)
    {
        // The caller holds outputMutex and has checked the pin is an output.
        const bool level = active != outputPins.at(pin).IsActiveLow();
        Check(gpio_set_level(static_cast<gpio_num_t>(pin), level ? 1 : 0), "setting", static_cast<gpio_num_t>(pin));
    }
}
