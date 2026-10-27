#pragma once

#include <any>
#include <string>
#include <type_traits>
#include <utility>

namespace ClankerCleaner::StateMachine
{
    class Flow;

    // Where a state is in its lifecycle.
    enum class StatePhase
    {
        None,
        Begin,
        Running,
        End,
        Abort
    };

    // What Run() asks the flow to do next. Most states return Stay or Next and leave
    // the routing to the flow's configuration; GoTo is for an explicit jump.
    //
    // Every transition except Stay can carry a context (any copyable value) for whatever
    // runs next; that state reads it with flow.GetContext<T>().
    class Transition
    {
    public:
        enum class Kind
        {
            Stay,
            Skip,
            Next,
            GoTo,
            Finish,
            Abort
        };

        static Transition Stay() { return Transition(Kind::Stay, {}, {}); }
        static Transition Skip(std::any context = {}) { return Transition(Kind::Skip, {}, std::move(context)); }
        static Transition Next(std::any context = {}) { return Transition(Kind::Next, {}, std::move(context)); }
        static Transition GoTo(std::string targetState, std::any context = {}) { return Transition(Kind::GoTo, std::move(targetState), std::move(context)); }
        static Transition Finish(std::any context = {}) { return Transition(Kind::Finish, {}, std::move(context)); }
        static Transition Abort(std::any context = {}) { return Transition(Kind::Abort, {}, std::move(context)); }

        Kind GetKind() const { return kind; }
        const std::string& GetTarget() const { return target; }
        const std::any& GetContext() const { return context; }

    private:
        friend class Flow;

        Transition(Kind kind, std::string target, std::any context)
            : kind(kind)
            , target(std::move(target))
            , context(std::move(context))
        {
        }

        Kind kind;
        std::string target;
        std::any context;
    };

    // One step of a flow. The flow calls Begin once on entry, then Run each tick
    // until it returns something other than Stay, then End. Abort replaces End
    // when the flow is aborted while this state is active.
    //
    // Next always means "move forward": from Begin it goes on to Run, from Run to the
    // next state. Stay means "not yet": Begin or Run is called again next tick.
    // Skip goes straight to the next state: from Begin it skips Run (End() still runs),
    // from Run it's the same as Next.
    // From Begin, GoTo and Finish also skip Run (End() still runs) and Abort calls Abort().
    // End returns false if it couldn't finish its cleanup; the flow then calls Abort()
    // on the same state and stops as Aborted.
    class State
    {
    public:
        explicit State(std::string name)
            : name(std::move(name))
        {
        }

        virtual ~State() = default;

        State(const State&) = delete;
        State& operator=(const State&) = delete;

        const std::string& GetName() const { return name; }

        virtual Transition Begin(Flow&) { return Transition::Next(); }
        virtual Transition Run(Flow& flow) = 0;
        virtual bool End(Flow&) { return true; }
        virtual void Abort(Flow&) {}

    private:
        std::string name;
    };


    // A state that only runs in TFlow: override the TFlow& overloads and the flow arrives
    // already cast. The Flow& overloads are final so they can't be overridden by mistake.
    template<typename TFlow>
    class StateWithFlow : public State
    {
    public:
        using State::State;

        virtual Transition Begin(TFlow& flow) { return State::Begin(static_cast<Flow&>(flow)); }
        Transition Begin(Flow& flow) final { return Begin(Cast(flow)); }

        virtual Transition Run(TFlow& flow) = 0;
        Transition Run(Flow& flow) final { return Run(Cast(flow)); }

        virtual bool End(TFlow& flow) { return State::End(static_cast<Flow&>(flow)); }
        bool End(Flow& flow) final { return End(Cast(flow)); }

        virtual void Abort(TFlow& flow) { State::Abort(static_cast<Flow&>(flow)); }
        void Abort(Flow& flow) final { Abort(Cast(flow)); }

    private:
        static TFlow& Cast(Flow& flow)
        {
            static_assert(std::is_base_of_v<Flow, TFlow>, "TFlow must derive from Flow");
            return static_cast<TFlow&>(flow);
        }
    };
}
