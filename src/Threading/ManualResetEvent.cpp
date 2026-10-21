#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::Threading
{
    ManualResetEvent::ManualResetEvent(bool initialState)
        : signalled(initialState)
    {
    }

    bool ManualResetEvent::Set()
    {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            signalled = true;
        }
        signalledCondition.notify_all();
        return true;
    }

    bool ManualResetEvent::Reset()
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        signalled = false;
        return true;
    }

    bool ManualResetEvent::IsSet() const
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        return signalled;
    }

    bool ManualResetEvent::WaitOne() const
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        signalledCondition.wait(lock, [this] { return signalled; });
        return true;
    }

    bool ManualResetEvent::WaitOne(int millisecondsTimeout) const
    {
        return WaitOne(std::chrono::milliseconds(millisecondsTimeout));
    }

    bool ManualResetEvent::WaitOne(std::stop_token stopToken) const
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        return signalledCondition.wait(lock, stopToken, [this] { return signalled; });
    }

    bool ManualResetEvent::WaitUntil(std::chrono::steady_clock::time_point deadline) const
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        return signalledCondition.wait_until(lock, deadline, [this] { return signalled; });
    }

    bool ManualResetEvent::WaitUntil(std::chrono::steady_clock::time_point deadline, std::stop_token stopToken) const
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        return signalledCondition.wait_until(lock, stopToken, deadline, [this] { return signalled; });
    }
}
