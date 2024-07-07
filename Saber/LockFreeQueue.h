#pragma once

#include <iostream>
#include <atomic>
#include <memory>

// Basic implementation of the Michael-Scott Lock-Free Queue
// https://www.cs.rochester.edu/~scott/papers/1996_PODC_queues.pdf
template<typename T>
class LockFreeQueue {
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
    LockFreeQueue() {
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
