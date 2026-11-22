#pragma once


#include <chrono>

namespace ClankerCleaner::Flows::Commands::Context
{
    struct FlushTankContext
    {
        public:
            static FlushTankContext Get(bool wasDirtyTankFlowRunning)
            {
                return FlushTankContext(wasDirtyTankFlowRunning);
            }
        
        bool WasDirtyTankFlowRunning() const
        {
            return wasDirtyTankFlowRunning;
        }
        
        bool highWater = false;
        std::chrono::steady_clock::time_point highWaterPumpStart;
        std::chrono::steady_clock::time_point flushPumpStart;
        std::chrono::steady_clock::time_point flushRinseLastStart;

        private:
            FlushTankContext(bool wasDirtyTankFlowRunning)
                : wasDirtyTankFlowRunning(wasDirtyTankFlowRunning)
            {
            };
            bool wasDirtyTankFlowRunning = false;
    };
}