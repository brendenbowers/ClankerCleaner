#pragma once

#include <chrono>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "StateMachine/Flow.h"
#include "Threading/ManualResetEvent.h"

namespace ClankerCleaner::StateMachine
{
    // Runs a set of named flows concurrently on a single thread. Each tick it steps every
    // running flow once, in the order they were added; Wake() triggers a step early.
    class Machine
    {
    public:
        explicit Machine(std::chrono::milliseconds tickInterval = std::chrono::milliseconds(100));
        ~Machine();

        Machine(const Machine&) = delete;
        Machine& operator=(const Machine&) = delete;

        // Add flows before Start(). Flows are stepped in the order added.
        template<typename TFlow = Flow>
        TFlow& AddFlow(std::string name);
        template<typename TFlow = Flow>
        TFlow* FindFlow(std::string_view name) const;
                
        std::vector<Flow*> GetFlows() const;

        // Starts and stops the machine thread. Stop() aborts any running flows first.
        bool Start();
        void Stop();
        bool IsStarted() const { return worker.joinable(); }

        // Safe from any thread, including from inside a state (e.g. an emergency flow).
        bool StartFlow(std::string_view name);
        void StartAll();
        bool AbortFlow(std::string_view name);
        void AbortAll();
        void AbortAllExcept(std::string_view name);

        void Wake() { wakeEvent.Set(); }

        // True once no flow is running or waiting to start; false if the timeout passes first.
        bool WaitUntilIdle(std::chrono::milliseconds timeout) const { return idleEvent.WaitOne(timeout); }

    private:
        friend class Flow;

        void FlowRequested();
        void RunLoop(std::stop_token stopToken);

        std::chrono::milliseconds tickInterval;
        std::vector<std::unique_ptr<Flow>> flows;

        Threading::ManualResetEvent wakeEvent;
        Threading::ManualResetEvent idleEvent{true};

        // Last member, so it stops and joins before everything above is destroyed.
        std::jthread worker;
    };

    template<typename TFlow>
    TFlow& Machine::AddFlow(std::string name)
    {
        static_assert(std::is_base_of_v<Flow, TFlow>, "TFlow must derive from Flow");

        if (IsStarted())
        {
            throw std::logic_error("Machine: can't add flow '" + name + "' while started");
        }
        if (FindFlow(name))
        {
            throw std::logic_error("Machine: duplicate flow '" + name + "'");
        }
        flows.push_back(std::make_unique<TFlow>(*this, std::move(name)));
        return static_cast<TFlow&>(*flows.back());
    }

    template<typename TFlow>
    TFlow* Machine::FindFlow(std::string_view name) const
    {
        static_assert(std::is_base_of_v<Flow, TFlow>, "TFlow must derive from Flow");
        for (const auto& flow : flows)
        {
            if (flow->GetName() == name)
            {
                return static_cast<TFlow*>(flow.get());
            }
        }
        return nullptr;
    }
}
