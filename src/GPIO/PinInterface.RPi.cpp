// Raspberry Pi half of PinInterface (libgpiod 2.x): claiming lines, the edge monitor
// thread and driving outputs. The shared half is in PinInterface.cpp.

#include "GPIO/PinInterface.h"
#include "GPIO/Platform.RPi.h"

#include <chrono>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <thread>

namespace ClankerCleaner::GPIO
{
    namespace
    {
        constexpr auto EdgeWaitTimeout = std::chrono::milliseconds(250);
    }

    struct PinInterface::PlatformState
    {
        std::optional<gpiod::line_request> InputRequest;
        std::optional<gpiod::line_request> OutputRequest;

        // Last member, so it stops before the requests it reads are destroyed.
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
        gpiod::chip gpioChip(Platform::ChipPath);

        if(!outputPins.empty())
        {
            PlatformLineConfig config;
            for(const auto& pinEntry : outputPins)
            {
                pinEntry.second.Configure(config);
            }
            platformState->OutputRequest.emplace(gpioChip.prepare_request()
                .set_consumer(Platform::Consumer)
                .set_line_config(config.Lines)
                .do_request());
        }

        // Requesting zero lines throws, so only watch when there are inputs.
        if(monitoredPins.empty())
        {
            return;
        }

        PlatformLineConfig config;
        for(const auto& pinEntry : monitoredPins)
        {
            pinEntry.second.Configure(config);
        }
        platformState->InputRequest.emplace(gpioChip.prepare_request()
            .set_consumer(Platform::Consumer)
            .set_line_config(config.Lines)
            .do_request());

        platformState->MonitorThread = std::jthread([this](std::stop_token stopToken)
        {
            gpiod::line_request& request = *platformState->InputRequest;
            gpiod::edge_event_buffer eventBuffer(32);

            while(!stopToken.stop_requested())
            {
                if(!request.wait_edge_events(EdgeWaitTimeout))
                {
                    continue;
                }

                request.read_edge_events(eventBuffer);
                for(const gpiod::edge_event& event : eventBuffer)
                {
                    const Edge edge = event.type() == gpiod::edge_event::event_type::RISING_EDGE ? Edge::Rising : Edge::Falling;
                    OnEdge(event.line_offset(), edge, event.timestamp_ns(), static_cast<std::uint32_t>(event.line_seqno()));
                }
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
        platformState->InputRequest.reset();
        platformState->OutputRequest.reset();
    }

    void PinInterface::PlatformSetOutput(std::uint32_t pin, bool active)
    {
        if(!platformState->OutputRequest)
        {
            throw std::logic_error("GPIO outputs aren't requested");
        }
        platformState->OutputRequest->set_value(pin, active ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE);
    }
}
