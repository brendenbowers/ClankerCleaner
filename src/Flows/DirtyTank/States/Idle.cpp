#include "Flows/DirtyTank/States/Idle.h"
#include "Flows/DirtyTank/DirtyTank.h"
#include "Flows/DirtyTank/Constants.h"
#include "GPIO/Pin.h"
#include "GPIO/Constants.h"


namespace ClankerCleaner::Flows::DirtyTank::States
{
    Idle::Idle()
        : State(Constants::State::IDLE_STATE)
        , lastEvent(0)
    {
    }

    Transition Idle::Begin(Flow& flow)
    {

        DirtyTankFlow& dirtyTankFlow = static_cast<DirtyTankFlow&>(flow);
        if(!dirtyTankFlow.settings || !dirtyTankFlow.settings->Dreame.Enabled || !dirtyTankFlow.settings->Dreame.Push || !dirtyTankFlow.dockMonitor)
        {
            return Transition::Next();
        }

        DockStatusChangedEventId = dirtyTankFlow.dockMonitor->AddChangeListener(std::bind(&Idle::HandleDockStatusChanged, this, std::placeholders::_1, std::placeholders::_2));

        
        return Transition::Next();
    }

    Transition Idle::Run(Flow &flow)
    {
        DirtyTankFlow& dirtyTankFlow = static_cast<DirtyTankFlow&>(flow);

        if(DockStatusChangedEventId != std::numeric_limits<Dreame::DockMonitor::ListenerId>::max())
        {
            if(Previous.IsWashingMop() && !Current.IsWashingMop())
            {
                return Transition::Next();
            }
        }

        
        // todo: maybe an optimization checking first if there are changes
        // GetSince includes events at exactly lastEvent, so +1 skips the one already handled.
        std::vector<GPIO::PinState> changedPins = dirtyTankFlow.pinMonitor->GetSince(lastEvent + 1);

        const std::uint32_t sensorPin = dirtyTankFlow.settings->Gpio.DirtyTankSensorPin;
        auto it = std::find_if(changedPins.begin(), changedPins.end(), 
            [sensorPin](GPIO::PinState pinState) { return pinState.PinId == sensorPin; });
        if(it == changedPins.end())
        {
            return Transition::Stay();
        }        

        std::uint64_t debounceTime = (std::chrono::steady_clock::now() -  dirtyTankFlow.settings->DirtyTank.SensorDebounce)
            .time_since_epoch()
            .count();
        if(it->ChangedNS > debounceTime)
        {
            // delay a bit to avoid false triggers from the sensor bouncing
            return Transition::Stay();
        }

        GPIO::PinState& pinState = *it;
        lastEvent = pinState.ChangedNS;

        if(pinState.Triggered)
        {
            return Transition::Next();
        }

        return Transition::Stay();        
    }

    bool Idle::End(Flow& flow)
    {
        UnregisterDockStatusChanged(flow);
        return true;
    }

    void Idle::Abort(Flow& flow)
    {
        UnregisterDockStatusChanged(flow);
    }

    void Idle::HandleDockStatusChanged(const Dreame::DockStatus& prev, const Dreame::DockStatus& cur)
    {
        Previous = prev;
        Current = cur;
    }

    void Idle::UnregisterDockStatusChanged(Flow& flow)
    {
        Current = Dreame::DockStatus();
        Previous = Dreame::DockStatus();

        DirtyTankFlow& dirtyTankFlow = static_cast<DirtyTankFlow&>(flow);
        if(dirtyTankFlow.dockMonitor && DockStatusChangedEventId != std::numeric_limits<Dreame::DockMonitor::ListenerId>::max())
        {
            dirtyTankFlow.dockMonitor->RemoveChangeListener(DockStatusChangedEventId);
        }
    }

}