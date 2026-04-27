#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Disparity
{
    enum class EngineFramePhase : uint32_t
    {
        Input,
        Simulation,
        Physics,
        Animation,
        Audio,
        Rendering,
        Editor,
        Count
    };

    struct FrameSchedulerDiagnostics
    {
        uint32_t RegisteredTasks = 0;
        uint32_t ExecutedTasks = 0;
        uint32_t SkippedTasks = 0;
        uint32_t FrameCount = 0;
        std::array<uint32_t, static_cast<size_t>(EngineFramePhase::Count)> PhaseExecutionCounts = {};
    };

    struct FrameSchedulerContext
    {
        float DeltaSeconds = 0.0f;
        uint32_t FrameIndex = 0;
    };

    class FrameScheduler
    {
    public:
        using Task = std::function<void(const FrameSchedulerContext&)>;

        [[nodiscard]] uint32_t RegisterTask(
            EngineFramePhase phase,
            std::string name,
            Task task,
            int priority = 0,
            bool enabled = true);
        void SetTaskEnabled(uint32_t taskId, bool enabled);
        void Clear();

        void ExecuteFrame(float deltaSeconds);

        [[nodiscard]] FrameSchedulerDiagnostics GetDiagnostics() const;
        [[nodiscard]] const std::vector<std::string>& GetLastExecutionOrder() const;
        [[nodiscard]] static const char* PhaseName(EngineFramePhase phase);

    private:
        struct ScheduledTask
        {
            uint32_t Id = 0;
            EngineFramePhase Phase = EngineFramePhase::Simulation;
            std::string Name;
            Task Callback;
            int Priority = 0;
            bool Enabled = true;
        };

        uint32_t m_nextTaskId = 1;
        uint32_t m_frameIndex = 0;
        std::vector<ScheduledTask> m_tasks;
        std::vector<std::string> m_lastExecutionOrder;
        FrameSchedulerDiagnostics m_diagnostics;
    };
}
