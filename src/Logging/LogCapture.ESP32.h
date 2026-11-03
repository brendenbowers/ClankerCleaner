#pragma once

// Keeps a copy of everything the firmware logs, both ESP-IDF's log and the program's spdlog
// output, in a ring buffer for readers such as the telnet log server. The serial console still
// gets all of it.

#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ClankerCleaner::Logging::Capture
{
    // A reader's place in the log: the number of bytes logged before it.
    using Position = std::uint64_t;

    // Call first thing in app_main, on the main task: Run's logger writes to that task's stdout.
    void Start();

    // Where a new reader starts to get the oldest whole line still held.
    Position Oldest();

    // Copies up to `size` bytes from `position` on and advances it. A reader that fell more than
    // the whole buffer behind skips to the oldest byte still held, and `dropped` is set.
    std::size_t Read(Position& position, char* buffer, std::size_t size, bool& dropped);

    // `task` gets a task notification whenever something is logged.
    bool AddReader(TaskHandle_t task);
    void RemoveReader(TaskHandle_t task);
}
