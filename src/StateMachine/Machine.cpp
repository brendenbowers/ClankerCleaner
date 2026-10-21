#include "StateMachine/Machine.h"

#include <stdexcept>

namespace ClankerCleaner::StateMachine
{
    Machine::Machine(std::chrono::milliseconds tickInterval)
        : tickInterval(tickInterval)
    {
    }

    Machine::~Machine()
    {
        Stop();
    }

    std::vector<Flow*> Machine::GetFlows() const
    {
        std::vector<Flow*> result;
        result.reserve(flows.size());
        for (const auto& flow : flows)
        {
            result.push_back(flow.get());
        }
        return result;
    }

    bool Machine::Start()
    {
        if (IsStarted())
        {
            return false;
        }
        worker = std::jthread([this](std::stop_token stopToken) { RunLoop(stopToken); });
        return true;
    }

    void Machine::Stop()
    {
        if (!IsStarted())
        {
            return;
        }
        worker.request_stop();
        // Stopping from a state would join the machine thread from itself.
        if (worker.get_id() != std::this_thread::get_id())
        {
            worker.join();
        }
    }

    bool Machine::StartFlow(std::string_view name)
    {
        Flow* flow = FindFlow(name);
        return flow && flow->RequestStart();
    }

    void Machine::StartAll()
    {
        for (const auto& flow : flows)
        {
            flow->RequestStart();
        }
    }

    bool Machine::AbortFlow(std::string_view name)
    {
        Flow* flow = FindFlow(name);
        if (!flow)
        {
            return false;
        }
        flow->RequestAbort();
        return true;
    }

    void Machine::AbortAll()
    {
        for (const auto& flow : flows)
        {
            flow->RequestAbort();
        }
    }

    void Machine::AbortAllExcept(std::string_view name)
    {
        for (const auto& flow : flows)
        {
            if (flow->GetName() != name)
            {
                flow->RequestAbort();
            }
        }
    }

    void Machine::FlowRequested()
    {
        idleEvent.Reset();
        Wake();
    }

    void Machine::RunLoop(std::stop_token stopToken)
    {
        while (!stopToken.stop_requested())
        {
            // Reset before stepping, so a Wake() raised during a step isn't lost.
            wakeEvent.Reset();

            bool anyActive = false;
            for (const auto& flow : flows)
            {
                flow->Step();
                anyActive = anyActive || flow->IsRunning() || flow->IsStartPending();
            }

            if (anyActive)
            {
                idleEvent.Reset();
            }
            else
            {
                idleEvent.Set();
            }

            wakeEvent.WaitOne(tickInterval, stopToken);
        }

        // Shutting down: give every running flow's current state its Abort.
        for (const auto& flow : flows)
        {
            flow->RequestAbort();
            flow->Step();
        }
        idleEvent.Set();
    }
}
