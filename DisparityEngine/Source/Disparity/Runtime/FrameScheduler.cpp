#include "Disparity/Runtime/FrameScheduler.h"

#include <algorithm>

namespace Disparity
{
    uint32_t FrameScheduler::RegisterTask(
        EngineFramePhase phase,
        std::string name,
        Task task,
        int priority,
        bool enabled)
    {
        if (!task || name.empty())
        {
            return 0;
        }

        const uint32_t id = m_nextTaskId++;
        m_tasks.push_back({ id, phase, std::move(name), std::move(task), priority, enabled });
        m_diagnostics.RegisteredTasks = static_cast<uint32_t>(m_tasks.size());
        return id;
    }

    void FrameScheduler::SetTaskEnabled(uint32_t taskId, bool enabled)
    {
        const auto found = std::find_if(m_tasks.begin(), m_tasks.end(), [taskId](const ScheduledTask& task) {
            return task.Id == taskId;
        });
        if (found != m_tasks.end())
        {
            found->Enabled = enabled;
        }
    }

    void FrameScheduler::Clear()
    {
        m_tasks.clear();
        m_lastExecutionOrder.clear();
        m_diagnostics = {};
        m_frameIndex = 0;
    }

    void FrameScheduler::ExecuteFrame(float deltaSeconds)
    {
        ++m_frameIndex;
        ++m_diagnostics.FrameCount;
        m_lastExecutionOrder.clear();

        std::vector<ScheduledTask*> ordered;
        ordered.reserve(m_tasks.size());
        for (ScheduledTask& task : m_tasks)
        {
            ordered.push_back(&task);
        }

        std::stable_sort(ordered.begin(), ordered.end(), [](const ScheduledTask* a, const ScheduledTask* b) {
            if (a->Phase != b->Phase)
            {
                return static_cast<uint32_t>(a->Phase) < static_cast<uint32_t>(b->Phase);
            }
            return a->Priority < b->Priority;
        });

        const FrameSchedulerContext context{ deltaSeconds, m_frameIndex };
        for (ScheduledTask* task : ordered)
        {
            if (!task->Enabled)
            {
                ++m_diagnostics.SkippedTasks;
                continue;
            }

            task->Callback(context);
            ++m_diagnostics.ExecutedTasks;
            ++m_diagnostics.PhaseExecutionCounts[static_cast<size_t>(task->Phase)];
            m_lastExecutionOrder.push_back(std::string(PhaseName(task->Phase)) + ":" + task->Name);
        }
    }

    FrameSchedulerDiagnostics FrameScheduler::GetDiagnostics() const
    {
        FrameSchedulerDiagnostics diagnostics = m_diagnostics;
        diagnostics.RegisteredTasks = static_cast<uint32_t>(m_tasks.size());
        return diagnostics;
    }

    const std::vector<std::string>& FrameScheduler::GetLastExecutionOrder() const
    {
        return m_lastExecutionOrder;
    }

    const char* FrameScheduler::PhaseName(EngineFramePhase phase)
    {
        switch (phase)
        {
        case EngineFramePhase::Input: return "Input";
        case EngineFramePhase::Simulation: return "Simulation";
        case EngineFramePhase::Physics: return "Physics";
        case EngineFramePhase::Animation: return "Animation";
        case EngineFramePhase::Audio: return "Audio";
        case EngineFramePhase::Rendering: return "Rendering";
        case EngineFramePhase::Editor: return "Editor";
        default: return "Unknown";
        }
    }
}
