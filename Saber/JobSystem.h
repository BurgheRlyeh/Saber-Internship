#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "LockFreeQueue.h"

template <size_t ThreadsCount = 2, typename DurationType = std::chrono::milliseconds, size_t Duration = 5, size_t Passes = 512>
class JobSystem {
    std::vector<std::thread> m_threads{ ThreadsCount };
    ArrayLockFreeQueue<std::function<void()>> m_jobs{};
    std::atomic<bool> m_isRunning{};

public:
    ~JobSystem() {
        StopRunning();
    }

    void StartRunning() {
        m_isRunning.store(true);
        for (std::thread& thread : m_threads) {
            thread = std::thread([this]() { this->Worker(); });
        }
    }

    void StopRunning() {
        m_isRunning.store(false);
        for (std::thread& thread : m_threads) {
            thread.join();
        }
    }

    bool AddJob(std::function<void()> job) {
        return m_jobs.Enqueue(job);
    }

private:
    void Worker() {
        size_t passes{};

        while (m_isRunning.load()) {
            std::function<void()> job{};
            if (m_jobs.Dequeue(job)) {
                job();
                passes = 0;
            }
            else if (++passes >= Passes) {
                std::this_thread::sleep_for(DurationType(Duration));
            }
        }
    }
};
