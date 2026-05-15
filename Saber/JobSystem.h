/**
 * @file JobSystem.h
 * @brief A fixed-size thread pool that dispatches @c std::function jobs
 *        from a lock-free queue.
 */
#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <thread>
#include <vector>

#include "LockFreeQueue.h"

/**
 * @brief Fixed-size thread pool driven by a lock-free job queue.
 *
 * Worker threads spin on the queue, optionally sleeping after a configurable
 * number of empty passes to avoid burning CPU when idle.
 *
 * @tparam ThreadsCount Number of worker threads (default 2).
 * @tparam DurationType Chrono duration type used for the idle sleep (default milliseconds).
 * @tparam Duration     Sleep duration value; 0 means no sleep.
 * @tparam Passes       Number of empty dequeue attempts before sleeping; 0 means always sleep.
 */
template <size_t ThreadsCount = 2, typename DurationType = std::chrono::milliseconds, size_t Duration = 0, size_t Passes = 0>
class JobSystem {
    std::vector<std::thread> m_threads{ ThreadsCount };
    ArrayLockFreeQueue<std::function<void()>> m_jobs{};
    std::atomic<bool> m_isRunning{};

public:
    /** @brief Stops all worker threads on destruction. */
    ~JobSystem() {
        StopRunning();
    }

    /**
     * @brief Spawns all worker threads and begins processing jobs.
     *
     * Must be called before @ref AddJob.
     */
    void StartRunning() {
        m_isRunning.store(true);
        for (std::thread& thread : m_threads) {
            thread = std::thread([this]() { this->Worker(); });
        }
    }

    /**
     * @brief Signals workers to stop and blocks until all threads have exited.
     *
     * Jobs still in the queue when this is called may or may not execute
     * depending on thread scheduling.
     */
    void StopRunning() {
        m_isRunning.store(false);
        for (std::thread& thread : m_threads) {
            thread.join();
        }
    }

    /**
     * @brief Enqueues a job for execution by a worker thread.
     * @param job Callable to execute on a worker thread.
     * @return @c true if the job was enqueued; @c false if the queue was full.
     */
    bool AddJob(std::function<void()> job) {
        return m_jobs.Enqueue(job);
    }

private:
    /** @brief Worker thread loop: dequeues and executes jobs until @ref StopRunning is called. */
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
