#include "Disparity/Runtime/JobSystem.h"

#include <algorithm>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace Disparity
{
    namespace
    {
        std::mutex g_jobsMutex;
        std::condition_variable g_jobsAvailable;
        std::condition_variable g_jobsIdle;
        std::queue<std::function<void()>> g_jobs;
        std::vector<std::thread> g_workers;
        bool g_running = false;
        uint32_t g_activeJobs = 0;

        void WorkerLoop()
        {
            for (;;)
            {
                std::function<void()> job;
                {
                    std::unique_lock lock(g_jobsMutex);
                    g_jobsAvailable.wait(lock, [] {
                        return !g_running || !g_jobs.empty();
                    });

                    if (!g_running && g_jobs.empty())
                    {
                        return;
                    }

                    job = std::move(g_jobs.front());
                    g_jobs.pop();
                    ++g_activeJobs;
                }

                job();

                {
                    std::scoped_lock lock(g_jobsMutex);
                    --g_activeJobs;
                    if (g_jobs.empty() && g_activeJobs == 0)
                    {
                        g_jobsIdle.notify_all();
                    }
                }
            }
        }
    }

    void JobSystem::Initialize(uint32_t workerCount)
    {
        std::scoped_lock lock(g_jobsMutex);
        if (g_running)
        {
            return;
        }

        g_running = true;
        if (workerCount == 0)
        {
            const uint32_t hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
            workerCount = std::max(1u, hardwareThreads > 1u ? hardwareThreads - 1u : 1u);
        }

        g_workers.reserve(workerCount);
        for (uint32_t index = 0; index < workerCount; ++index)
        {
            g_workers.emplace_back(WorkerLoop);
        }
    }

    void JobSystem::Shutdown()
    {
        {
            std::scoped_lock lock(g_jobsMutex);
            if (!g_running)
            {
                return;
            }

            g_running = false;
        }

        g_jobsAvailable.notify_all();
        for (std::thread& worker : g_workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }

        g_workers.clear();
        std::queue<std::function<void()>> empty;
        {
            std::scoped_lock lock(g_jobsMutex);
            std::swap(g_jobs, empty);
            g_activeJobs = 0;
        }
    }

    void JobSystem::Dispatch(std::function<void()> job)
    {
        if (!job)
        {
            return;
        }

        bool runImmediately = false;
        {
            std::scoped_lock lock(g_jobsMutex);
            if (!g_running)
            {
                runImmediately = true;
            }
            else
            {
                g_jobs.push(std::move(job));
            }
        }

        if (runImmediately)
        {
            job();
            return;
        }

        g_jobsAvailable.notify_one();
    }

    void JobSystem::DispatchReadTextFile(std::filesystem::path path, std::function<void(AsyncTextReadResult)> callback)
    {
        Dispatch([path = std::move(path), callback = std::move(callback)]() mutable {
            AsyncTextReadResult result;
            result.Path = path;

            std::ifstream file(path);
            if (file)
            {
                std::ostringstream text;
                text << file.rdbuf();
                result.Text = text.str();
                result.Success = true;
            }

            if (callback)
            {
                callback(std::move(result));
            }
        });
    }

    void JobSystem::WaitIdle()
    {
        std::unique_lock lock(g_jobsMutex);
        g_jobsIdle.wait(lock, [] {
            return g_jobs.empty() && g_activeJobs == 0;
        });
    }

    uint32_t JobSystem::WorkerCount()
    {
        std::scoped_lock lock(g_jobsMutex);
        return static_cast<uint32_t>(g_workers.size());
    }

    bool JobSystem::IsRunning()
    {
        std::scoped_lock lock(g_jobsMutex);
        return g_running;
    }
}
