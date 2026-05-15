/**
 * @file LockFreeQueue.h
 * @brief Lock-free queue implementations: a linked-list variant and a
 *        bounded ring-buffer variant.
 */
#pragma once

#include <iostream>
#include <atomic>
#include <bit>
#include <memory>
#include <vector>
#include <thread>

/**
 * @brief Unbounded lock-free FIFO queue based on the Michael-Scott algorithm.
 *
 * Uses @c std::atomic<std::shared_ptr<Node>> so that the sentinel node is
 * automatically memory-managed without a separate memory reclaimer.
 *
 * @tparam T Element type; must be default-constructible.
 *
 * @note Reference: https://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf
 */
template<typename T>
class ListLockFreeQueue {
    struct Node {
        T data;
        std::atomic<std::shared_ptr<Node>> pNext;
        Node(T value)
            : data(value)
            , pNext(nullptr)
        {}
    };

    std::atomic<std::shared_ptr<Node>> m_pHead{};
    std::atomic<std::shared_ptr<Node>> m_pTail{};

public:
    /** @brief Initialises the queue with a sentinel node so head == tail == sentinel. */
    ListLockFreeQueue() {
        std::shared_ptr<Node> pSentinel{ std::make_shared<Node>(T()) };
        m_pHead.store(pSentinel);
        m_pTail.store(pSentinel);
    }

    /**
     * @brief Enqueues @p value at the tail of the queue.
     * @param value Value to enqueue.
     */
    void Enqueue(T value) {
        std::shared_ptr<Node> pNode{ std::make_shared<Node>(value) };
        std::shared_ptr<Node> pTail{};

        while (true) {
            pTail = m_pTail.load();
            std::shared_ptr<Node> next{ pTail->pNext };

            if (pTail == m_pTail.load()) {
                if (next == nullptr) {
                    if (pTail->pNext.compare_exchange_weak(next, pNode)) {
                        break;
                    }
                }
                else {
                    m_pTail.compare_exchange_weak(pTail, next);
                }
            }
        }
        m_pTail.compare_exchange_weak(pTail, pNode);
    }

    /**
     * @brief Dequeues the head element into @p result.
     * @param[out] result Receives the dequeued value on success.
     * @return @c true if an element was dequeued; @c false if the queue was empty.
     */
    bool Dequeue(T& result) {
        while (true) {
            std::shared_ptr<Node> pHead{ m_pHead.load() };
            std::shared_ptr<Node> pTail{ m_pTail.load() };
            std::shared_ptr<Node> pNext{ pHead->pNext };

            if (pHead == m_pHead.load()) {
                if (pHead == pTail) {
                    if (pNext == nullptr) {
                        return false;
                    }

                    m_pTail.compare_exchange_weak(pTail, pNext);
                }
                else {
                    result = pNext->data;

                    if (m_pHead.compare_exchange_weak(pHead, pNext)) {
                        break;
                    }
                }
            }
        }

        return true;
    }
};

/**
 * @brief Controls how the capacity is computed for @ref ArrayLockFreeQueue.
 */
enum class ArrayLockFreeQueueCapacityType {
    DegreeOfTwo, /**< @brief Round up to the next power of two; enables fast modulo via bitmask. */
    Exact        /**< @brief Use the exact capacity (one extra slot is reserved as sentinel). */
};

/**
 * @brief Bounded, lock-free ring-buffer FIFO queue.
 *
 * Supports multiple concurrent producers and consumers.  The capacity is
 * either rounded up to a power of two (for fast index masking) or kept exact.
 *
 * @tparam T                Element type.
 * @tparam Optimization     Whether to use power-of-two capacity (default).
 * @tparam DestructAfterPop Whether to call the element destructor when it is popped.
 *
 * @note Reference: https://www.codeproject.com/Articles/153898/Yet-another-implementation-of-a-lock-free-circul
 */
template <
    typename T,
    ArrayLockFreeQueueCapacityType Optimization = ArrayLockFreeQueueCapacityType::DegreeOfTwo,
    bool DestructAfterPop = false
>
class ArrayLockFreeQueue {
    T* m_data{};

    std::atomic<size_t> m_pushId{};
    std::atomic<size_t> m_popId{};
    std::atomic<size_t> m_lastDataId{};

    size_t m_capacity{};
    size_t m_capacityMask{}; /**< @brief Bitmask used with DegreeOfTwo optimisation. */

public:
    /**
     * @brief Constructs the queue with the given logical capacity.
     * @param capacity Maximum number of elements (rounded up to next power of two
     *                 when @p Optimization is DegreeOfTwo).
     */
    ArrayLockFreeQueue(size_t capacity = 255) {
        if constexpr (Optimization == ArrayLockFreeQueueCapacityType::DegreeOfTwo) {
            m_capacity = std::bit_ceil(capacity);
            m_capacityMask = m_capacity - 1;
            m_data = new T[m_capacity];
        }
        else {
            m_capacity = capacity + 1;
            m_data = new T[m_capacity];
        }
    }

    ~ArrayLockFreeQueue() {
        delete[] m_data;
    }

    /** @brief Returns the usable element capacity (internal size minus one sentinel slot). */
    size_t GetCapacity() const {
        return m_capacity - 1;
    }

    /** @brief Returns the approximate number of elements currently in the queue. */
    size_t GetSize() {
        size_t pushId{ m_pushId.load() };
        size_t popId{ m_popId.load() };

        return ToRingBufId(pushId - popId + m_capacity);
    }

    /** @brief Returns @c true if the queue contains no elements. */
    bool IsEmpty() {
        return m_popId.load() == m_lastDataId.load();
    }

    /** @brief Returns @c true if the queue cannot accept any more elements. */
    bool IsFull() {
        return m_popId.load() == ToRingBufId(m_pushId.load() + 1);
    }

    /**
     * @brief Enqueues @p data if the queue is not full.
     * @param data Element to enqueue (copy).
     * @return @c true on success; @c false if the queue was full.
     */
    bool Enqueue(const T& data) {
        size_t id;

        do {
            id = m_pushId.load();
            if (ToRingBufId(id + 1) == m_popId.load()) {
                return false;   // queue is full
            }
        } while (!m_pushId.compare_exchange_weak(id, ToRingBufId(id + 1)));

        m_data[id] = data;

        while (!m_lastDataId.compare_exchange_weak(id, ToRingBufId(id + 1))) {
            std::this_thread::yield();
        }

        return true;
    }

    /**
     * @brief Dequeues the front element into @p data.
     * @param[out] data Receives the element value on success.
     * @return @c true if an element was dequeued; @c false if the queue was empty.
     */
    bool Dequeue(T& data) {
        while (true) {
            size_t id{ m_popId.load() };

            if (id == m_lastDataId.load()) {
                return false;   // queue is empty
            }

            data = m_data[id];
            if (DestructAfterPop) {
                m_data[id].~T();
            }

            if (m_popId.compare_exchange_weak(id, ToRingBufId(id + 1))) {
                return true;
            }
        }

        return false;
    }

private:
    /**
     * @brief Wraps an index into the ring buffer's valid range.
     * @param id Raw index to wrap.
     * @return Wrapped index.
     */
    size_t ToRingBufId(size_t id) {
        if constexpr (Optimization == ArrayLockFreeQueueCapacityType::DegreeOfTwo)
            return id & m_capacityMask;
        else
            return id % m_capacity;
    }
};
