// ESP-IDF starts the program at app_main, on its main task, instead of main(), and Run runs
// right there. The main task isn't a pthread, so ESP-IDF's pthread_self() would assert on it
// (std::this_thread::get_id() calls it); the build wraps pthread_self (esp32/main/CMakeLists.txt)
// to give the main task an id of its own.

#include "Run.h"
#include "Logging/LogCapture.ESP32.h"
#include "Net/Network.ESP32.h"
#include "Net/TelnetLog.ESP32.h"

#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <thread>

#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <pthread.h>

namespace
{
    // Long enough to read the log before a restart, so a bad setting doesn't spin.
    constexpr auto RestartDelay = std::chrono::seconds(10);

    // Set first thing in app_main; nothing asks for a thread id on the main task before then.
    TaskHandle_t mainTask = nullptr;
}

extern "C" pthread_t __real_pthread_self();

// The main task's handle is its id. It can't clash with a pthread's, which is the address of
// ESP-IDF's own record for that thread.
extern "C" pthread_t __wrap_pthread_self()
{
    if(mainTask && xTaskGetCurrentTaskHandle() == mainTask)
    {
        return static_cast<pthread_t>(reinterpret_cast<std::uintptr_t>(mainTask));
    }
    return __real_pthread_self();
}

// ESP-IDF's C library declares signal() but doesn't provide it. Nothing sends signals here,
// so Run's SIGINT/SIGTERM handlers are just never called.
extern "C" _sig_func_ptr signal(int /*signalNumber*/, _sig_func_ptr /*handler*/)
{
    return SIG_DFL;
}

extern "C" void app_main()
{
    mainTask = xTaskGetCurrentTaskHandle();
    // First, so the log server's backlog has the network startup too.
    ClankerCleaner::Logging::Capture::Start();

    // Offline is fine: the tank flows don't need the network, only the cloud client does.
    ClankerCleaner::Net::Network::Start();
    ClankerCleaner::Net::TelnetLog::Start();

    // Run's stack is the main task's, CONFIG_ESP_MAIN_TASK_STACK_SIZE; threads it starts get
    // CONFIG_PTHREAD_TASK_STACK_SIZE_DEFAULT.
    char programName[] = "clanker_cleaner";
    char* arguments[] = {programName, nullptr};
    const int exitCode = ClankerCleaner::Run(1, arguments);

    // There's no OS to return to, so start over.
    std::printf("clanker_cleaner exited with %d; restarting in %lld s\n", exitCode,
                static_cast<long long>(RestartDelay.count()));
    std::this_thread::sleep_for(RestartDelay);
    esp_restart();
}
