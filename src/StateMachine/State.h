#pragma once

#include <string>
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
    class Transition
    {
    public:
        enum class Kind
        {
            Stay,
            Next,
            GoTo,
            Finish,
            Abort
        };

        static Transition Stay() { return Transition(Kind::Stay, {}); }
        static Transition Next() { return Transition(Kind::Next, {}); }
        static Transition GoTo(std::string targetState) { return Transition(Kind::GoTo, std::move(targetState)); }
        static Transition Finish() { return Transition(Kind::Finish, {}); }
        static Transition Abort() { return Transition(Kind::Abort, {}); }

        Kind GetKind() const { return kind; }
        const std::string& GetTarget() const { return target; }

    private:
        Transition(Kind kind, std::string target)
            : kind(kind)
            , target(std::move(target))
        {
        }

        Kind kind;
        std::string target;
    };

    // One step of a flow. The flow calls Begin once on entry, then Run each tick
    // until it returns something other than Stay, then End. Abort replaces End
    // when the flow is aborted while this state is active.
    //
    // Next always means "move forward": from Begin it goes on to Run, from Run to the
    // next state. Stay means "not yet": Begin or Run is called again next tick.
    // From Begin, GoTo and Finish skip Run (End() still runs) and Abort calls Abort().
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
}
