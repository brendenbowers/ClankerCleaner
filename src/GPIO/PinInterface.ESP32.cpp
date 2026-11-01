// ESP32 half of PinInterface (ESP-IDF GPIO driver): configuring pins, the edge monitor
// thread and driving outputs. The shared half is in PinInterface.cpp.
//
// Each input's interrupt fires on both edges and queues the pin and time; the monitor
// thread then reads the pin's level, so an edge that bounced straight back is dropped.

#include "GPIO/PinInterface.h"
#include "GPIO/Platform.ESP32.h"

#include <chrono>
#include <deque>
#include <map>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>

#include <esp_attr.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace ClankerCleaner::GPIO
{
    namespace
    {
        constexpr auto EdgeWaitTimeout = std::chrono::milliseconds(250);
        constexpr UBaseType_t EdgeQueueLength = 32;

        struct EdgeEvent
        {
            gpio_num_t Pin;
            // esp_timer time, the same clock as std::chrono::steady_clock.
            std::int64_t TimeUs;
        };

        // Handed to the interrupt handler, so it must stay put while the handler is added.
        struct InterruptContext
        {
            gpio_num_t Pin;
            QueueHandle_t Queue;
        };

        void IRAM_ATTR OnInterrupt(void* argument)
        {
            const auto* context = static_cast<const InterruptContext*>(argument);
            const EdgeEvent event{context->Pin, esp_timer_get_time()};
            BaseType_t higherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(context->Queue, &event, &higherPriorityTaskWoken);
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
        QueueHandle_t EdgeQueue = nullptr;
        std::deque<InterruptContext> Interrupts;
        std::map<gpio_num_t, int> Levels;

        // Last member, so it stops before the queue it reads is deleted.
        std::jthread MonitorThread;
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

        platformState->EdgeQueue = xQueueCreate(EdgeQueueLength, sizeof(EdgeEvent));
        if(!platformState->EdgeQueue)
        {
            throw std::runtime_error("GPIO edge queue couldn't be created");
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

            InterruptContext& context = platformState->Interrupts.emplace_back(InterruptContext{gpio, platformState->EdgeQueue});
            Check(gpio_isr_handler_add(gpio, OnInterrupt, &context), "watching", gpio);
        }

        platformState->MonitorThread = std::jthread([this](std::stop_token stopToken)
        {
            const TickType_t waitTicks = pdMS_TO_TICKS(EdgeWaitTimeout.count());
            std::map<gpio_num_t, std::uint32_t> sequences;
            EdgeEvent event;

            while(!stopToken.stop_requested())
            {
                if(xQueueReceive(platformState->EdgeQueue, &event, waitTicks) != pdTRUE)
                {
                    continue;
                }

                const int level = gpio_get_level(event.Pin);
                int& lastLevel = platformState->Levels[event.Pin];
                if(level == lastLevel)
                {
                    continue;
                }
                lastLevel = level;

                OnEdge(event.Pin, EdgeForLevel(level), static_cast<std::uint64_t>(event.TimeUs) * 1000, ++sequences[event.Pin]);
            }
        });
    }

    void PinInterface::PlatformStop()
    {
        if(platformState->MonitorThread.joinable())
        {
            platformState->MonitorThread.request_stop();
            platformState->MonitorThread.join();
        }

        for(const InterruptContext& context : platformState->Interrupts)
        {
            gpio_isr_handler_remove(context.Pin);
            gpio_reset_pin(context.Pin);
        }
        platformState->Interrupts.clear();
        platformState->Levels.clear();

        if(platformState->EdgeQueue)
        {
            vQueueDelete(platformState->EdgeQueue);
            platformState->EdgeQueue = nullptr;
        }

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
