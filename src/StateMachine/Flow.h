#pragma once

#include <any>
#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "StateMachine/State.h"

namespace ClankerCleaner::StateMachine
{
    class Machine;

    enum class FlowStatus
    {
        Idle,
        Running,
        Finished,
        Aborted
    };

    // A named sequence of states (e.g. dirty tank, clean tank, emergency). Flows don't own
    // a thread: the Machine's thread steps every flow in turn, so states must not block in Run().
    class Flow
    {
    public:
        using PhaseListener = std::function<void(const Flow& flow, const State& state, StatePhase phase)>;

        Flow(Machine& machine, std::string name);
        // Virtual so flows derived from Flow are destroyed correctly through the Machine.
        virtual ~Flow() = default;

        Flow(const Flow&) = delete;
        Flow& operator=(const Flow&) = delete;

        // Add states before starting the flow. The first state added is the initial one, and
        // Transition::Next() follows the order they were added unless SetNext overrides it.
        State& AddState(std::unique_ptr<State> state);

        template <typename TState, typename... Args>
        TState& AddState(Args&&... args)
        {
            static_assert(std::is_base_of_v<State, TState>, "TState must derive from State");
            return static_cast<TState&>(AddState(std::make_unique<TState>(std::forward<Args>(args)...)));
        }

        void SetInitialState(std::string_view stateName);

        // Where Transition::Next() goes from fromState, instead of the next state added.
        void SetNext(std::string_view fromState, std::string_view toState);

        // When true, Next() from the last state added goes back to the initial state
        // instead of finishing the flow.
        void SetLooping(bool loops) { looping = loops; }

        // Called on the machine thread whenever a state changes phase; handy for logging.
        void SetPhaseListener(PhaseListener listener);

        // Safe from any thread; the machine thread acts on them at its next step.
        bool RequestStart();
        void RequestAbort();

        // Steps the machine now instead of waiting for its next tick, e.g. from a pin event.
        void Wake();

        const std::string& GetName() const { return name; }
        Machine& GetMachine() const { return machine; }

        FlowStatus GetStatus() const;
        bool IsRunning() const { return GetStatus() == FlowStatus::Running; }
        bool IsStartPending() const { return startRequested; }
        std::string GetCurrentStateName() const;
        StatePhase GetCurrentPhase() const;
        std::string GetLastError() const;

        // The context from the transition into the current state; nullptr if there was none
        // or it isn't a TContext. Machine thread (states) only. After Finish or Abort it holds
        // that transition's context until the flow starts again.
        template <typename TContext>
        TContext* GetContext() { return std::any_cast<TContext>(&context); }
        template <typename TContext>
        const TContext* GetContext() const { return std::any_cast<TContext>(&context); }
        bool HasContext() const { return context.has_value(); }

    private:
        friend class Machine;

        // Machine thread only.
        void Step();
        State* FindState(std::string_view stateName) const;
        State* ResolveNext(const State& fromState) const;
        void ApplyTransition(Transition transition);
        void MoveTo(State* nextState, std::any nextContext);
        void EnterPhase(State& state, StatePhase phase);
        void AbortCurrentState();
        void SetStatus(FlowStatus newStatus);

        Machine& machine;
        std::string name;

        std::map<std::string, std::unique_ptr<State>, std::less<>> states;
        std::vector<State*> stateOrder;
        std::map<std::string, std::string, std::less<>> nextOverrides;
        std::string initialStateName;
        bool looping = false;
        PhaseListener phaseListener;

        std::atomic<bool> startRequested{false};
        std::atomic<bool> abortRequested{false};

        // Machine thread only.
        State* currentState = nullptr;
        bool beginPending = false;
        bool beginReported = false;
        std::any context;

        mutable std::mutex statusMutex;
        FlowStatus status = FlowStatus::Idle;
        std::string currentStateName;
        StatePhase currentPhase = StatePhase::None;
        std::string lastError;
    };
}
