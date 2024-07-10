#pragma once

#include <iostream>
#include <atomic>
#include <memory>
#include <vector>
#include <thread>

// Basic implementation of the Michael-Scott Lock-Free Queue
// https://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf
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
    ListLockFreeQueue() {
        std::shared_ptr<Node> pSentinel{ std::make_shared<Node>(T()) };
        m_pHead.store(pSentinel);
        m_pTail.store(pSentinel);
    }

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

enum ArrayLockFreeQueueOptimizationType {
    SPEED, MEMORY
};

// Bounded Ring Buffer Lock-Free Queue
// https://www.codeproject.com/Articles/153898/Yet-another-implementation-of-a-lock-free-circul
template <typename T, ArrayLockFreeQueueOptimizationType Optimization = SPEED>
class ArrayLockFreeQueue {
    std::vector<T> m_data{};

    std::atomic<size_t> m_pushId{};
    std::atomic<size_t> m_popId{};
    std::atomic<size_t> m_lastDataId{};

    size_t m_capacity{};
    size_t m_capacityMask{};

public:
    ArrayLockFreeQueue(size_t capacity = 255) {
        if (Optimization == SPEED) {
            m_capacityMask = capacity;
            for (size_t i{ 1 }; i < sizeof(size_t) + 1; i <<= 1) {
                m_capacityMask |= m_capacityMask >> i;
            }
            m_capacity = m_capacityMask + 1;

            m_data.resize(m_capacity);
        }
        else {
            m_capacity = capacity + 1;
            m_data.resize(m_capacity);
        }
    }

    size_t GetCapacity() const {
        return m_capacity - 1;
    }

    size_t GetSize() {
        size_t pushId{ m_pushId.load() };
        size_t popId{ m_popId.load() };

        return ToRingBufId(pushId - popId + m_capacity);
    }

    bool IsEmpty() {
        return m_popId.load() == m_lastDataId.load();
    }

    bool IsFull() {
        return m_popId.load() == ToRingBufId(m_pushId.load() + 1);
    }

    bool Enqueue(const T& data) {
        size_t id;

        do {
            id = m_pushId.load();
            if (ToRingBufId(id + 1) == m_popId.load()) {
                return false;   // queue is full
            }
        } while (!m_pushId.compare_exchange_weak(id, ToRingBufId(id + 1)));

        m_data.at(id) = data;

        while (!m_lastDataId.compare_exchange_weak(id, ToRingBufId(id + 1))) {
            std::this_thread::yield();
        }

        return true;
    }

    bool Dequeue(T& data) {
        while (true) {
            size_t id{ m_popId.load() };

            if (id == m_lastDataId.load()) {
                return false;   // queue is empty
            }

            data = m_data.at(id);
            m_data.at(id).~T();

            if (m_popId.compare_exchange_weak(id, ToRingBufId(id + 1))) {
                return true;
            }
        }

        return false;
    }

private:
    size_t ToRingBufId(size_t id) {
        return Optimization == SPEED ? id & m_capacityMask : id % m_capacity;
    }
};