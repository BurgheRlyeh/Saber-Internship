#pragma once

#include "Headers.h"

#include <deque>
#include <limits>
#include <queue>

inline constexpr uint64_t DEFAULT_FENCE_VALUE{ std::numeric_limits<uint64_t>::max() };

template <typename T>
struct FencedData {
	uint64_t fenceValue{ DEFAULT_FENCE_VALUE };
	T data{};
};

template <typename T>
class FencedQueue {
	// TODO: use vector-based deque with reserved capacity
	//       or specify collection type as template parameter
	std::queue<FencedData<T>> m_data{};

public:
	void Push(uint64_t fenceValue, T&& data) {
		m_data.push(FencedData<T>{ fenceValue, std::move(data) });
	}

	std::vector<T> PopCompleted(uint64_t completedFenceValue) {
		std::vector<T> popped{};
		while (!m_data.empty() && m_data.front().fenceValue <= completedFenceValue) {
			popped.push_back(std::move(m_data.front().data));
			m_data.pop();
		}
		return popped;
	}
};

template <typename T>
class FrameFencedQueue : public FencedQueue<T> {
public:
	void FinishFrame(uint64_t fenceValue, uint64_t completedFenceValue) {
		this->Push(fenceValue, ProduceForPush());
		for (auto& data : this->PopCompleted(completedFenceValue)) {
			BeforePop(data);
		}
	}
protected:
	virtual T ProduceForPush() = 0;
	virtual void BeforePop(const T&) {}
};

template <typename T>
class FrameDataBuffer : public FrameFencedQueue<std::vector<T>> {
	size_t m_initCapacity{};
	std::vector<T> m_curr{};

public:
	explicit FrameDataBuffer(size_t initCapacity = 16) :
		m_initCapacity(initCapacity)
	{
		Reserve();
	}

	void Reserve(size_t capacity) {
		m_curr.reserve(capacity);
	}
	void ReserveForAll(size_t capacity) {
		m_initCapacity = capacity;
		Reserve();
	}

	void Add(const T& data) {
		m_curr.push_back(data);
	}
	void Add(T&& data) {
		m_curr.push_back(std::move(data));
	}

protected:
	void Reserve() {
		m_curr.reserve(m_initCapacity);
	}

	virtual std::vector<T> ProduceForPush() override {
		auto forPush{ std::move(m_curr) };

		m_curr = std::vector<T>();
		Reserve();

		return forPush;
	}
};
