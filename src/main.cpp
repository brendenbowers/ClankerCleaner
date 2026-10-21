#include <gpiod.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstdint>

namespace PIConfiguration
{
    constexpr const char* CONSUMER = "ClankerCleaner";
    constexpr const char* CHIP = "/dev/gpiochip0";
    constexpr std::uint32_t HIGH_WASTE_WATER_TANK = 22;
}

int main()
{

    gpiod::chip gpioChip(PIConfiguration::CHIP);

    const gpiod::line_settings pin22Settings = gpiod::line_settings()
        .set_direction(gpiod::line::direction::INPUT)
        .set_edge_detection(gpiod::line::edge::BOTH)
        .set_bias(gpiod::line::bias::PULL_UP);

    gpiod::line_config config = gpiod::line_config();
        config.add_line_settings(PIConfiguration::HIGH_WASTE_WATER_TANK, pin22Settings);

   
    gpiod::line_request clankerCleanerRequest = gpioChip.prepare_request()
        .set_consumer(PIConfiguration::CONSUMER)
        .set_line_config(config)
        .do_request();


    gpiod::edge_event_buffer eventBuffer(32);

    while (true)
    {
        if(clankerCleanerRequest.wait_edge_events(std::chrono::seconds(1)))
        {
            clankerCleanerRequest.read_edge_events(eventBuffer);

            for(const auto& event : eventBuffer)
            {
                auto eventType = event.type();

                const char* waterDetection = eventType == gpiod::edge_event::event_type::RISING_EDGE ? "NO WATER" : "WATER";

                std::cout << "Detected " << waterDetection << std::endl;
            }

        }

    }
    


    return EXIT_SUCCESS;
}
