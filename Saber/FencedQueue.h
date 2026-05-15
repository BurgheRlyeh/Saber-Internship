/**
 * @file FencedQueue.h
 * @brief Queue that associates data with GPU fence values for deferred release.
 *
 * Provides @ref FencedQueue (base template) and @ref FrameDataBuffer
 * (specialisation for per-frame resource lists).
 */
#pragma once

#include "Headers.h"

#include <deque>
#include <limits>
#include <queue>

/**
 * @brief Associates arbitrary data with GPU fence values to enable deferred release.
 *
 * Each call to @c FinishFrame(fenceValue, ...) pushes a snapshot of the
 * current data paired with @p fenceValue.  When @c ReleaseCompletedFrames()
 * detects that the GPU has reached or passed that fence value, the data is
 * popped (and @ref BeforePop is called for cleanup).
 *
 * Subclasses must implement @ref ProduceForPush to supply the data snapshot.
 *
 * @tparam T Type of data stored per frame.
 */
template <typename T>
class FencedQueue {
protected:
    /** @brief Sentinel value indicating a slot that has not yet been assigned a fence value. */
    static constexpr size_t DEFAULT_FENCE_VALUE{ std::numeric_limits<uint64_t>::max() };

    /** @brief Data entry paired with the fence value at which it can be released. */
    struct FencedData {
        uint64_t fenceValue{ DEFAULT_FENCE_VALUE }; /**< @brief GPU fence completion value. */
        T data{};                                    /**< @brief Stored payload. */
    };

    std::queue<FencedData> m_data; /**< @brief FIFO of pending frame payloads. */

public:
    /**
     * @brief Constructs the queue, pre-populating it with @p numFrames empty slots.
     * @param numFrames Number of frames-in-flight; controls queue depth.
     */
    FencedQueue(size_t numFrames) : m_data(std::deque<FencedData>(numFrames)) {
        // TODO: HACK: use vector-based deque with reserved capacity
        for (size_t i{}; i < numFrames; ++i) {
            m_data.pop();
        }
    }
    virtual ~FencedQueue() = default;

    /**
     * @brief Records the current frame's data and releases completed frames.
     * @param fenceValue           Fence value that will be signalled when this frame's
     *                             GPU work is complete.
     * @param completedFenceValue  Highest fence value the GPU has already completed.
     */
    void FinishFrame(uint64_t fenceValue, uint64_t completedFenceValue) {
        FinishCurrentFrame(fenceValue);
        ReleaseCompletedFrames(completedFenceValue);
    }

protected:
    /**
     * @brief Subclass hook that produces the data snapshot pushed for this frame.
     * @return Data to associate with the current frame's fence value.
     */
    virtual T ProduceForPush() = 0;

    /**
     * @brief Pushes the current frame's snapshot with the supplied fence value.
     * @param fenceValue Fence value to attach to this frame's data.
     */
    virtual void FinishCurrentFrame(uint64_t fenceValue) {
        m_data.push(FencedData{ fenceValue, ProduceForPush() });
    }

    /**
     * @brief Called just before a completed frame's data is popped from the queue.
     * @param data Data that is about to be released.
     */
    virtual void BeforePop(const T& data) {}

    /**
     * @brief Pops and releases all frames whose fence values are @c <= @p completedFenceValue.
     * @param completedFenceValue Highest fence value the GPU has completed.
     */
    virtual void ReleaseCompletedFrames(uint64_t completedFenceValue) {
        while (!m_data.empty() && m_data.front().fenceValue <= completedFenceValue) {
            BeforePop(m_data.front().data);
            m_data.pop();
        }
    }
};

/**
 * @brief Per-frame accumulation buffer that holds a list of resources until the
 *        GPU finishes using them.
 *
 * Typical use: accumulate intermediate GPU resources during a frame, then call
 * @c FinishFrame() so they are released once the GPU no longer references them.
 *
 * @tparam T Element type stored in the per-frame vector.
 */
template <typename T>
class FrameDataBuffer : public FencedQueue<std::vector<T>> {
    size_t m_initCapacity{};    /**< @brief Initial reserved capacity for each frame's vector. */
    std::vector<T> m_curr{};   /**< @brief Accumulation buffer for the current in-flight frame. */

public:
    /**
     * @brief Constructs the buffer with a given number of frames and initial capacity.
     * @param numFrames    Depth of the fence queue.
     * @param initCapacity Initial @c reserve() size for each per-frame vector.
     */
    FrameDataBuffer(size_t numFrames, size_t initCapacity = 16) :
        FencedQueue<std::vector<T>>(numFrames),
        m_initCapacity(initCapacity)
    {
        Reserve();
    }

    /**
     * @brief Reserves memory in the current frame's accumulation vector.
     * @param capacity Minimum element capacity to reserve.
     */
    void Reserve(size_t capacity) {
        m_curr.reserve(capacity);
    }

    /**
     * @brief Updates the default reservation size and re-reserves the current vector.
     * @param capacity New initial capacity applied from this frame onwards.
     */
    void ReserveForAll(size_t capacity) {
        m_initCapacity = capacity;
        Reserve();
    }

    /**
     * @brief Appends an element (copy) to the current frame's accumulation list.
     * @param data Element to add.
     */
    void Add(const T& data) {
        m_curr.push_back(data);
    }

    /**
     * @brief Appends an element (move) to the current frame's accumulation list.
     * @param data Element to move-add.
     */
    void Add(T&& data) {
        m_curr.push_back(std::forward(data));
    }

protected:
    /** @brief Reserves the accumulation vector to @ref m_initCapacity. */
    void Reserve() {
        m_curr.reserve(m_initCapacity);
    }

    /** @brief Moves the current accumulation vector out and resets it for the next frame. */
    virtual std::vector<T> ProduceForPush() override {
        auto forPush{ std::move(m_curr) };

        m_curr = std::vector<T>();
        Reserve();

        return forPush;
    }
};
