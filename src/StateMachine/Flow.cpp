#include "StateMachine/Flow.h"
#include "StateMachine/Machine.h"

#include <exception>
#include <stdexcept>

#include "Logging/Logging.h"

namespace ClankerCleaner::StateMachine
{
    Flow::Flow(Machine& machine, std::string name)
        : machine(machine)
        , name(std::move(name))
    {
    }

    State& Flow::AddState(std::unique_ptr<State> state)
    {
        if (!state)
        {
            throw std::invalid_argument("Flow '" + name + "': state is null");
        }
        if (IsRunning())
        {
            throw std::logic_error("Flow '" + name + "': can't add states while running");
        }

        const std::string stateName = state->GetName();
        auto [entry, inserted] = states.emplace(stateName, std::move(state));
        if (!inserted)
        {
            throw std::logic_error("Flow '" + name + "': duplicate state '" + stateName + "'");
        }
        stateOrder.push_back(entry->second.get());
        if (initialStateName.empty())
        {
            initialStateName = stateName;
        }
        return *entry->second;
    }

    void Flow::SetNext(std::string_view fromState, std::string_view toState)
    {
        if (!FindState(fromState) || !FindState(toState))
        {
            throw std::invalid_argument("Flow '" + name + "': unknown state in SetNext('" +
                                        std::string(fromState) + "', '" + std::string(toState) + "')");
        }
        nextOverrides.insert_or_assign(std::string(fromState), std::string(toState));
    }

    void Flow::SetInitialState(std::string_view stateName)
    {
        if (!FindState(stateName))
        {
            throw std::invalid_argument("Flow '" + name + "': unknown state '" + std::string(stateName) + "'");
        }
        initialStateName = stateName;
    }

    void Flow::SetPhaseListener(PhaseListener listener)
    {
        phaseListener = std::move(listener);
    }

    bool Flow::RequestStart()
    {
        {
            auto logger = Logging::Get("StateMachineFlow");
            logger->info("Flow {} starting with states:", GetName());
            for(const State* state : stateOrder)
            {
                logger->info(state->GetName());
            }
        }


        if (IsRunning() || !FindState(initialStateName))
        {
            return false;
        }
        startRequested = true;
        machine.FlowRequested();
        return true;
    }

    void Flow::RequestAbort()
    {
        startRequested = false;
        abortRequested = true;
        machine.Wake();
    }

    void Flow::Wake()
    {
        machine.Wake();
    }

    FlowStatus Flow::GetStatus() const
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return status;
    }

    std::string Flow::GetCurrentStateName() const
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return currentStateName;
    }

    StatePhase Flow::GetCurrentPhase() const
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return currentPhase;
    }

    std::string Flow::GetLastError() const
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        return lastError;
    }

    void Flow::Step()
    {
        if (abortRequested.exchange(false))
        {
            if (currentState)
            {
                AbortCurrentState();
            }
            return;
        }

        if (startRequested.exchange(false) && !currentState)
        {
            currentState = FindState(initialStateName);
            beginPending = true;
            beginReported = false;
            context.reset();
            {
                std::lock_guard<std::mutex> lock(statusMutex);
                lastError.clear();
            }
            SetStatus(FlowStatus::Running);
        }

        if (!currentState)
        {
            return;
        }

        try
        {
            if (beginPending)
            {
                // Begin may Stay over several ticks; report the phase once.
                if (!beginReported)
                {
                    beginReported = true;
                    EnterPhase(*currentState, StatePhase::Begin);
                }

                Transition begun = currentState->Begin(*this);
                switch (begun.GetKind())
                {
                case Transition::Kind::Stay:
                    return;
                case Transition::Kind::Next:
                    beginPending = false;
                    // Begin can hand its own Run a context; without one, Run sees the incoming one.
                    if (begun.context.has_value())
                    {
                        context = std::move(begun.context);
                    }
                    EnterPhase(*currentState, StatePhase::Running);
                    break;

                default:
                    beginPending = false;
                    ApplyTransition(std::move(begun));
                    return;
                }
            }

            ApplyTransition(currentState->Run(*this));
        }
        catch (const std::exception& error)
        {
            {
                std::lock_guard<std::mutex> lock(statusMutex);
                lastError = error.what();
            }
            AbortCurrentState();
        }
    }

    State* Flow::FindState(std::string_view stateName) const
    {
        auto entry = states.find(stateName);
        return entry == states.end() ? nullptr : entry->second.get();
    }

    // SetNext override first, then the state added after this one, then the initial state
    // when looping. nullptr means the flow finishes.
    State* Flow::ResolveNext(const State& fromState) const
    {
        auto overrideEntry = nextOverrides.find(fromState.GetName());
        if (overrideEntry != nextOverrides.end())
        {
            return FindState(overrideEntry->second);
        }

        for (std::size_t stateIndex = 0; stateIndex < stateOrder.size(); ++stateIndex)
        {
            if (stateOrder[stateIndex] == &fromState)
            {
                if (stateIndex + 1 < stateOrder.size())
                {
                    return stateOrder[stateIndex + 1];
                }
                break;
            }
        }
        return looping ? FindState(initialStateName) : nullptr;
    }

    void Flow::ApplyTransition(Transition transition)
    {
        switch (transition.GetKind())
        {
        case Transition::Kind::Stay:
            return;

        case Transition::Kind::Abort:
            // Set after Abort() so the aborted state still sees the context it was entered with.
            AbortCurrentState();
            context = std::move(transition.context);
            return;

        case Transition::Kind::Finish:
            MoveTo(nullptr, std::move(transition.context));
            return;

        case Transition::Kind::Skip:
        case Transition::Kind::Next:
            MoveTo(ResolveNext(*currentState), std::move(transition.context));
            return;

        case Transition::Kind::GoTo:
        {
            State* nextState = FindState(transition.GetTarget());
            if (!nextState)
            {
                throw std::logic_error("unknown state '" + transition.GetTarget() + "'");
            }
            MoveTo(nextState, std::move(transition.context));
            return;
        }
        }
    }

    // Ends the current state and either begins nextState with nextContext or, for nullptr,
    // finishes the flow. A failed End aborts the flow instead.
    void Flow::MoveTo(State* nextState, std::any nextContext)
    {
        EnterPhase(*currentState, StatePhase::End);
        if (!currentState->End(*this))
        {
            {
                std::lock_guard<std::mutex> lock(statusMutex);
                lastError = "state '" + currentState->GetName() + "' failed to end";
            }
            AbortCurrentState();
            return;
        }

        // Replaced even when empty, so a state never sees a context meant for an earlier one.
        context = std::move(nextContext);
        if (!nextState)
        {
            currentState = nullptr;
            SetStatus(FlowStatus::Finished);
            return;
        }

        currentState = nextState;
        beginPending = true;
        beginReported = false;
        // Begin the next state on the next pass rather than waiting a full tick.
        machine.Wake();
    }

    void Flow::EnterPhase(State& state, StatePhase phase)
    {
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            currentStateName = state.GetName();
            currentPhase = phase;
        }
        if (phaseListener)
        {
            phaseListener(*this, state, phase);
        }
    }

    void Flow::AbortCurrentState()
    {
        State* abortedState = currentState;
        currentState = nullptr;
        beginPending = false;

        try
        {
            EnterPhase(*abortedState, StatePhase::Abort);
            abortedState->Abort(*this);
        }
        catch (const std::exception& error)
        {
            std::lock_guard<std::mutex> lock(statusMutex);
            if (lastError.empty())
            {
                lastError = error.what();
            }
        }
        SetStatus(FlowStatus::Aborted);
    }

    void Flow::SetStatus(FlowStatus newStatus)
    {
        std::lock_guard<std::mutex> lock(statusMutex);
        status = newStatus;
    }
}
