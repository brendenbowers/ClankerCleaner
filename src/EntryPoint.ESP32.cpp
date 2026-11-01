// ESP-IDF starts the program at app_main, on its main task, instead of main(). Run goes on
// a std::thread of its own: the main task isn't a pthread, and std::this_thread::get_id()
// (which spdlog calls for every message) asserts there.

#include "Run.h"
#include "Net/Network.ESP32.h"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <thread>

#include <esp_pthread.h>
#include <esp_system.h>

namespace
{
    // Run's thread; threads it starts get CONFIG_PTHREAD_TASK_STACK_SIZE_DEFAULT.
    constexpr std::size_t RunStackSize = 16 * 1024;
    // Long enough to read the log before a restart, so a bad setting doesn't spin.
    constexpr auto RestartDelay = std::chrono::seconds(10);
}

// ESP-IDF's C library declares signal() but doesn't provide it. Nothing sends signals here,
// so Run's SIGINT/SIGTERM handlers are just never called.
extern "C" _sig_func_ptr signal(int /*signalNumber*/, _sig_func_ptr /*handler*/)
{
    return SIG_DFL;
}

extern "C" void app_main()
{
    // Offline is fine: the tank flows don't need the network, only the cloud client does.
    ClankerCleaner::Net::Network::Start();

    // Applies only to threads this task creates.
    esp_pthread_cfg_t threadConfig = esp_pthread_get_default_config();
    threadConfig.stack_size = RunStackSize;
    threadConfig.thread_name = "clanker_cleaner";
    esp_pthread_set_cfg(&threadConfig);

    int exitCode = 0;
    std::thread runThread([&exitCode]
    {
        char programName[] = "clanker_cleaner";
        char* arguments[] = {programName, nullptr};
        exitCode = ClankerCleaner::Run(1, arguments);
    });
    runThread.join();

    // There's no OS to return to, so start over.
    std::printf("clanker_cleaner exited with %d; restarting in %lld s\n", exitCode,
                static_cast<long long>(RestartDelay.count()));
    std::this_thread::sleep_for(RestartDelay);
    esp_restart();
}
