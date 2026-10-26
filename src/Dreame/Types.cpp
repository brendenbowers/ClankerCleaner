#include "Dreame/Types.h"

#include <algorithm>
#include <format>
#include <initializer_list>
#include <string_view>
#include <utility>

namespace ClankerCleaner::Dreame
{
    namespace
    {
        template <typename TEnum>
        using NameTable = std::initializer_list<std::pair<TEnum, std::string_view>>;

        // Values missing from the table print as their number.
        template <typename TEnum>
        std::string LookUp(TEnum value, NameTable<TEnum> names)
        {
            for (const auto& [entry, name] : names)
            {
                if (entry == value)
                {
                    return std::string(name);
                }
            }
            return std::to_string(static_cast<std::int32_t>(value));
        }

        template <typename TEnum>
        bool IsOneOf(TEnum value, std::initializer_list<TEnum> candidates)
        {
            return std::find(candidates.begin(), candidates.end(), value) != candidates.end();
        }

        bool IsOneOf(std::int32_t value, std::initializer_list<std::int32_t> candidates)
        {
            return std::find(candidates.begin(), candidates.end(), value) != candidates.end();
        }
    }

    bool DockStatus::IsCleaningJobActive() const
    {
        return Task != TaskStatus::Unknown && Task != TaskStatus::Completed;
    }

    bool DockStatus::IsRobotCleaning() const
    {
        return IsOneOf(Robot, {RobotState::Sweeping, RobotState::Mopping, RobotState::SweepingAndMopping,
                               RobotState::SpotCleaning, RobotState::SecondCleaning, RobotState::ExtraCleaning,
                               RobotState::CleanSummon}) ||
               IsOneOf(Status, {RobotStatus::Cleaning, RobotStatus::PartialCleaning, RobotStatus::SegmentCleaning,
                                RobotStatus::ZoneCleaning, RobotStatus::SpotCleaning});
    }

    bool DockStatus::IsBaseBusy() const
    {
        return IsOneOf(Base, {BaseStatus::Washing, BaseStatus::CleanAddWater, BaseStatus::AddingWater}) ||
               IsOneOf(Robot, {RobotState::Washing, RobotState::CleanAddWater, RobotState::WaterCheck,
                               RobotState::Draining, RobotState::AutoWaterDraining, RobotState::StationCleaning,
                               RobotState::AutoEmptying, RobotState::Emptying});
    }

    bool DockStatus::IsCleanTankEmpty() const
    {
        return CleanTank == CleanWaterTank::LowWater ||
               IsOneOf(LowWater, {LowWaterWarning::NoWaterLeftDismiss, LowWaterWarning::NoWaterLeft,
                                  LowWaterWarning::NoWaterLeftAfterClean, LowWaterWarning::NoWaterForClean,
                                  LowWaterWarning::LowWater}) ||
               IsOneOf(ErrorCode, {ErrorCodes::WaterBoxEmpty, ErrorCodes::WaterTankDry, ErrorCodes::CleanTankLevel});
    }

    bool DockStatus::IsDirtyTankFull() const
    {
        return DirtyTank == DirtyWaterTank::NotInstalledOrFull ||
               IsOneOf(ErrorCode, {ErrorCodes::DirtyWaterTankFull, ErrorCodes::DirtyTankLevel,
                                   ErrorCodes::DirtyTankNotInstalled});
    }

    bool DockStatus::SameStateAs(const DockStatus& other) const
    {
        return Online == other.Online && Robot == other.Robot && Status == other.Status && Task == other.Task &&
               Base == other.Base && CleanTank == other.CleanTank && DirtyTank == other.DirtyTank &&
               LowWater == other.LowWater && ErrorCode == other.ErrorCode;
    }

    std::string ToString(RobotState value)
    {
        return LookUp<RobotState>(value, {
            {RobotState::Unknown, "unknown"}, {RobotState::Sweeping, "sweeping"}, {RobotState::Idle, "idle"},
            {RobotState::Paused, "paused"}, {RobotState::Error, "error"}, {RobotState::Returning, "returning"},
            {RobotState::Charging, "charging"}, {RobotState::Mopping, "mopping"}, {RobotState::Drying, "drying"},
            {RobotState::Washing, "washing"}, {RobotState::ReturningToWash, "returning to wash"},
            {RobotState::Building, "building map"}, {RobotState::SweepingAndMopping, "sweeping and mopping"},
            {RobotState::ChargingCompleted, "charged"}, {RobotState::Upgrading, "upgrading"},
            {RobotState::CleanSummon, "summon clean"}, {RobotState::StationReset, "station reset"},
            {RobotState::ReturningInstallMop, "returning to install mop"},
            {RobotState::ReturningRemoveMop, "returning to remove mop"}, {RobotState::WaterCheck, "water check"},
            {RobotState::CleanAddWater, "adding clean water"}, {RobotState::WashingPaused, "washing paused"},
            {RobotState::AutoEmptying, "auto emptying"}, {RobotState::RemoteControl, "remote control"},
            {RobotState::SmartCharging, "smart charging"}, {RobotState::SecondCleaning, "second cleaning"},
            {RobotState::HumanFollowing, "following"}, {RobotState::SpotCleaning, "spot cleaning"},
            {RobotState::ReturningAutoEmpty, "returning to auto empty"},
            {RobotState::WaitingForTask, "waiting for task"}, {RobotState::StationCleaning, "station cleaning"},
            {RobotState::ReturningToDrain, "returning to drain"}, {RobotState::Draining, "draining"},
            {RobotState::AutoWaterDraining, "auto water draining"}, {RobotState::Emptying, "emptying"},
            {RobotState::DustBagDrying, "dust bag drying"}, {RobotState::DustBagDryingPaused, "dust bag drying paused"},
            {RobotState::HeadingToExtraCleaning, "heading to extra cleaning"},
            {RobotState::ExtraCleaning, "extra cleaning"},
        });
    }

    std::string ToString(RobotStatus value)
    {
        return LookUp<RobotStatus>(value, {
            {RobotStatus::Unknown, "unknown"}, {RobotStatus::Idle, "idle"}, {RobotStatus::Paused, "paused"},
            {RobotStatus::Cleaning, "cleaning"}, {RobotStatus::BackHome, "returning home"},
            {RobotStatus::PartialCleaning, "partial cleaning"}, {RobotStatus::FollowWall, "following wall"},
            {RobotStatus::Charging, "charging"}, {RobotStatus::Ota, "updating"}, {RobotStatus::Fct, "factory test"},
            {RobotStatus::WifiSet, "wifi setup"}, {RobotStatus::PowerOff, "powered off"},
            {RobotStatus::Factory, "factory"}, {RobotStatus::Error, "error"},
            {RobotStatus::RemoteControl, "remote control"}, {RobotStatus::Sleeping, "sleeping"},
            {RobotStatus::SelfRepair, "self repair"}, {RobotStatus::FactoryFunctionTest, "factory function test"},
            {RobotStatus::Standby, "standby"}, {RobotStatus::SegmentCleaning, "room cleaning"},
            {RobotStatus::ZoneCleaning, "zone cleaning"}, {RobotStatus::SpotCleaning, "spot cleaning"},
            {RobotStatus::FastMapping, "fast mapping"}, {RobotStatus::CruisingPath, "cruising path"},
            {RobotStatus::CruisingPoint, "cruising point"}, {RobotStatus::SummonClean, "summon clean"},
            {RobotStatus::Shortcut, "shortcut"}, {RobotStatus::PersonFollow, "following"},
            {RobotStatus::WaterCheck, "water check"},
        });
    }

    std::string ToString(TaskStatus value)
    {
        return LookUp<TaskStatus>(value, {
            {TaskStatus::Unknown, "unknown"}, {TaskStatus::Completed, "none"},
            {TaskStatus::AutoCleaning, "auto cleaning"}, {TaskStatus::ZoneCleaning, "zone cleaning"},
            {TaskStatus::SegmentCleaning, "room cleaning"}, {TaskStatus::SpotCleaning, "spot cleaning"},
            {TaskStatus::FastMapping, "fast mapping"}, {TaskStatus::AutoCleaningPaused, "auto cleaning paused"},
            {TaskStatus::ZoneCleaningPaused, "zone cleaning paused"},
            {TaskStatus::SegmentCleaningPaused, "room cleaning paused"},
            {TaskStatus::SpotCleaningPaused, "spot cleaning paused"},
            {TaskStatus::MapCleaningPaused, "map cleaning paused"}, {TaskStatus::DockingPaused, "docking paused"},
            {TaskStatus::MoppingPaused, "mopping paused"},
            {TaskStatus::SegmentMoppingPaused, "room mopping paused"},
            {TaskStatus::ZoneMoppingPaused, "zone mopping paused"},
            {TaskStatus::AutoMoppingPaused, "auto mopping paused"},
            {TaskStatus::AutoDockingPaused, "auto docking paused"},
            {TaskStatus::SegmentDockingPaused, "room docking paused"},
            {TaskStatus::ZoneDockingPaused, "zone docking paused"}, {TaskStatus::CruisingPath, "cruising path"},
            {TaskStatus::CruisingPathPaused, "cruising path paused"}, {TaskStatus::CruisingPoint, "cruising point"},
            {TaskStatus::CruisingPointPaused, "cruising point paused"},
            {TaskStatus::SummonCleanPaused, "summon clean paused"},
            {TaskStatus::ReturningInstallMop, "returning to install mop"},
            {TaskStatus::ReturningRemoveMop, "returning to remove mop"},
            {TaskStatus::StationCleaning, "station cleaning"},
        });
    }

    std::string ToString(BaseStatus value)
    {
        return LookUp<BaseStatus>(value, {
            {BaseStatus::Unknown, "unknown"}, {BaseStatus::Idle, "idle"}, {BaseStatus::Washing, "washing"},
            {BaseStatus::Drying, "drying"}, {BaseStatus::Returning, "returning"}, {BaseStatus::Paused, "paused"},
            {BaseStatus::CleanAddWater, "adding clean water"}, {BaseStatus::AddingWater, "adding water"},
            {BaseStatus::ReturningForDryMop, "returning to dry mop"},
        });
    }

    std::string ToString(CleanWaterTank value)
    {
        return LookUp<CleanWaterTank>(value, {
            {CleanWaterTank::Unknown, "unknown"}, {CleanWaterTank::Installed, "ok"},
            {CleanWaterTank::NotInstalled, "not installed"}, {CleanWaterTank::LowWater, "low"},
            {CleanWaterTank::Checking, "checking"},
        });
    }

    std::string ToString(DirtyWaterTank value)
    {
        return LookUp<DirtyWaterTank>(value, {
            {DirtyWaterTank::Unknown, "unknown"}, {DirtyWaterTank::Installed, "ok"},
            {DirtyWaterTank::NotInstalledOrFull, "full or removed"},
        });
    }

    std::string ToString(LowWaterWarning value)
    {
        return LookUp<LowWaterWarning>(value, {
            {LowWaterWarning::Unknown, "unknown"}, {LowWaterWarning::NoWarning, "none"},
            {LowWaterWarning::NoWaterLeftDismiss, "no water left (dismissed)"},
            {LowWaterWarning::NoWaterLeft, "no water left"},
            {LowWaterWarning::NoWaterLeftAfterClean, "no water left after clean"},
            {LowWaterWarning::NoWaterForClean, "no water for clean"}, {LowWaterWarning::LowWater, "low water"},
            {LowWaterWarning::TankNotInstalled, "tank not installed"},
        });
    }

    std::string Describe(const DockStatus& status)
    {
        if (!status.Online && status.UpdatedAt == std::chrono::system_clock::time_point{})
        {
            return "no data yet";
        }
        return std::format("{}robot {} ({}), task {}, base {}, clean tank {}, dirty tank {}, low water {}, error {}",
                           status.Online ? "" : "offline, last seen: ", ToString(status.Robot),
                           ToString(status.Status), ToString(status.Task), ToString(status.Base),
                           ToString(status.CleanTank), ToString(status.DirtyTank), ToString(status.LowWater),
                           status.ErrorCode);
    }
}
