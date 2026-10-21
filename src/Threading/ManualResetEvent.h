#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stop_token>

namespace ClankerCleaner::Threading
{
    // Works like C#'s System.Threading.ManualResetEvent: Set() releases every waiter
    // and the event stays signalled until Reset().
    class ManualResetEvent
    {
    public:
        explicit ManualResetEvent(bool initialState = false);

        ManualResetEvent(const ManualResetEvent&) = delete;
        ManualResetEvent& operator=(const ManualResetEvent&) = delete;

        bool Set();
        bool Reset();
        bool IsSet() const;

        bool WaitOne() const;

        // Like C#: -1 waits forever; returns false if the timeout passes first.
        bool WaitOne(int millisecondsTimeout) const;

        template <typename Rep, typename Period>
        bool WaitOne(std::chrono::duration<Rep, Period> timeout) const
        {
            if (timeout < std::chrono::duration<Rep, Period>::zero())
            {
                return WaitOne();
            }
            return WaitUntil(std::chrono::steady_clock::now() + std::chrono::ceil<std::chrono::steady_clock::duration>(timeout));
        }

        // Also returns false when the stop token is triggered, e.g. from a std::jthread.
        bool WaitOne(std::stop_token stopToken) const;

        template <typename Rep, typename Period>
        bool WaitOne(std::chrono::duration<Rep, Period> timeout, std::stop_token stopToken) const
        {
            if (timeout < std::chrono::duration<Rep, Period>::zero())
            {
                return WaitOne(stopToken);
            }
            return WaitUntil(std::chrono::steady_clock::now() + std::chrono::ceil<std::chrono::steady_clock::duration>(timeout), stopToken);
        }

    private:
        bool WaitUntil(std::chrono::steady_clock::time_point deadline) const;
        bool WaitUntil(std::chrono::steady_clock::time_point deadline, std::stop_token stopToken) const;

        // Waiting doesn't change the event's state, so const waits may still lock and wait on these.
        mutable std::mutex stateMutex;
        mutable std::condition_variable_any signalledCondition;
        bool signalled;
    };
}
