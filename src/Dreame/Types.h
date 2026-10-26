#pragma once

#include <chrono>
#include <compare>
#include <cstdint>
#include <string>

// Values the Dreame/MOVA cloud reports for a robot and its base station. Property IDs and
// values come from the Tasshack/dreame-vacuum integration (dreame/types.py). Values a model
// adds later still fit in the enums; ToString prints them as numbers.
namespace ClankerCleaner::Dreame
{
    // A MIoT property: service ID and property ID within it, written "siid.piid".
    struct PropertyId
    {
        std::int32_t ServiceId = 0;
        std::int32_t PropertyNumber = 0;

        auto operator<=>(const PropertyId&) const = default;
    };

    namespace Properties
    {
        constexpr PropertyId RobotState{2, 1};
        constexpr PropertyId Error{2, 2};
        constexpr PropertyId RobotStatus{4, 1};
        constexpr PropertyId TaskStatus{4, 7};
        constexpr PropertyId BaseStatus{4, 25};
        constexpr PropertyId LowWaterWarning{4, 41};
        constexpr PropertyId CleanWaterTank{27, 1};
        constexpr PropertyId DirtyWaterTank{27, 2};
    }

    // What the robot is doing (2.1), the most detailed of the three robot properties.
    enum class RobotState : std::int32_t
    {
        Unknown = -1,
        Sweeping = 1,
        Idle = 2,
        Paused = 3,
        Error = 4,
        Returning = 5,
        Charging = 6,
        Mopping = 7,
        Drying = 8,
        Washing = 9,
        ReturningToWash = 10,
        Building = 11,
        SweepingAndMopping = 12,
        ChargingCompleted = 13,
        Upgrading = 14,
        CleanSummon = 15,
        StationReset = 16,
        ReturningInstallMop = 17,
        ReturningRemoveMop = 18,
        WaterCheck = 19,
        CleanAddWater = 20,
        WashingPaused = 21,
        AutoEmptying = 22,
        RemoteControl = 23,
        SmartCharging = 24,
        SecondCleaning = 25,
        HumanFollowing = 26,
        SpotCleaning = 27,
        ReturningAutoEmpty = 28,
        WaitingForTask = 29,
        StationCleaning = 30,
        ReturningToDrain = 31,
        Draining = 32,
        AutoWaterDraining = 33,
        Emptying = 34,
        DustBagDrying = 35,
        DustBagDryingPaused = 36,
        HeadingToExtraCleaning = 37,
        ExtraCleaning = 38,
    };

    // Coarser robot status (4.1).
    enum class RobotStatus : std::int32_t
    {
        Unknown = -1,
        Idle = 0,
        Paused = 1,
        Cleaning = 2,
        BackHome = 3,
        PartialCleaning = 4,
        FollowWall = 5,
        Charging = 6,
        Ota = 7,
        Fct = 8,
        WifiSet = 9,
        PowerOff = 10,
        Factory = 11,
        Error = 12,
        RemoteControl = 13,
        Sleeping = 14,
        SelfRepair = 15,
        FactoryFunctionTest = 16,
        Standby = 17,
        SegmentCleaning = 18,
        ZoneCleaning = 19,
        SpotCleaning = 20,
        FastMapping = 21,
        CruisingPath = 22,
        CruisingPoint = 23,
        SummonClean = 24,
        Shortcut = 25,
        PersonFollow = 26,
        WaterCheck = 1501,
    };

    // The cleaning job (4.7); Completed when there's none.
    enum class TaskStatus : std::int32_t
    {
        Unknown = -1,
        Completed = 0,
        AutoCleaning = 1,
        ZoneCleaning = 2,
        SegmentCleaning = 3,
        SpotCleaning = 4,
        FastMapping = 5,
        AutoCleaningPaused = 6,
        ZoneCleaningPaused = 7,
        SegmentCleaningPaused = 8,
        SpotCleaningPaused = 9,
        MapCleaningPaused = 10,
        DockingPaused = 11,
        MoppingPaused = 12,
        SegmentMoppingPaused = 13,
        ZoneMoppingPaused = 14,
        AutoMoppingPaused = 15,
        AutoDockingPaused = 16,
        SegmentDockingPaused = 17,
        ZoneDockingPaused = 18,
        CruisingPath = 20,
        CruisingPathPaused = 21,
        CruisingPoint = 22,
        CruisingPointPaused = 23,
        SummonCleanPaused = 24,
        ReturningInstallMop = 25,
        ReturningRemoveMop = 26,
        StationCleaning = 27,
    };

    // The self-wash base station (4.25).
    enum class BaseStatus : std::int32_t
    {
        Unknown = -1,
        Idle = 0,
        Washing = 1,
        Drying = 2,
        Returning = 3,
        Paused = 4,
        CleanAddWater = 5,
        AddingWater = 6,
        ReturningForDryMop = 7,
    };

    enum class CleanWaterTank : std::int32_t
    {
        Unknown = -1,
        Installed = 0,
        NotInstalled = 1,
        LowWater = 2,
        Checking = 3,
    };

    // The dock can't tell a removed dirty tank from a full one.
    enum class DirtyWaterTank : std::int32_t
    {
        Unknown = -1,
        Installed = 0,
        NotInstalledOrFull = 1,
    };

    enum class LowWaterWarning : std::int32_t
    {
        Unknown = -1,
        NoWarning = 0,
        NoWaterLeftDismiss = 1,
        NoWaterLeft = 2,
        NoWaterLeftAfterClean = 3,
        NoWaterForClean = 4,
        LowWater = 5,
        TankNotInstalled = 6,
    };

    // Error codes (2.2) that matter here; 0 is no error.
    namespace ErrorCodes
    {
        constexpr std::int32_t None = 0;
        constexpr std::int32_t WaterBoxEmpty = 10;
        constexpr std::int32_t DirtyTankNotInstalled = 76;
        constexpr std::int32_t DirtyWaterTankFull = 86;
        constexpr std::int32_t WaterTankDry = 107;
        constexpr std::int32_t CleanTankLevel = 116;
        constexpr std::int32_t DirtyTankLevel = 118;
    }

    // One poll's worth of robot and dock state. Values a model doesn't report stay Unknown.
    struct DockStatus
    {
        // False until the first successful poll, and again while the cloud is unreachable;
        // the other fields then hold the last values received.
        bool Online = false;
        std::chrono::system_clock::time_point UpdatedAt{};

        RobotState Robot = RobotState::Unknown;
        RobotStatus Status = RobotStatus::Unknown;
        TaskStatus Task = TaskStatus::Unknown;
        BaseStatus Base = BaseStatus::Unknown;
        CleanWaterTank CleanTank = CleanWaterTank::Unknown;
        DirtyWaterTank DirtyTank = DirtyWaterTank::Unknown;
        LowWaterWarning LowWater = LowWaterWarning::Unknown;
        std::int32_t ErrorCode = -1;

        // A cleaning job is under way, including returns to the dock to wash mops mid-job.
        bool IsCleaningJobActive() const;
        // The robot is out on the floor cleaning right now.
        bool IsRobotCleaning() const;
        // The base is washing mops or moving water, so draining or filling would interfere.
        bool IsBaseBusy() const;
        bool IsCleanTankEmpty() const;
        // Also true while the dirty tank is lifted out; the dock reports both the same way.
        bool IsDirtyTankFull() const;

        // Compares everything except UpdatedAt, to detect changes between polls.
        bool SameStateAs(const DockStatus& other) const;
    };

    std::string ToString(RobotState value);
    std::string ToString(RobotStatus value);
    std::string ToString(TaskStatus value);
    std::string ToString(BaseStatus value);
    std::string ToString(CleanWaterTank value);
    std::string ToString(DirtyWaterTank value);
    std::string ToString(LowWaterWarning value);

    // One line for the log, e.g. "robot charging, task completed, base idle, clean tank ok, ...".
    std::string Describe(const DockStatus& status);
}
