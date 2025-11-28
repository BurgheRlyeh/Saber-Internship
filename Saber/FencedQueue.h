#pragma once

#include "Headers.h"

#include <deque>
#include <limits>
#include <queue>

template <typename T>
class FencedQueue {
protected:
	static constexpr size_t DEFAULT_FENCE_VALUE{ std::numeric_limits<uint64_t>::max() };
	struct FencedData {
		uint64_t fenceValue{ DEFAULT_FENCE_VALUE };
		T data{};
	};
	std::queue<FencedData> m_data;

public:
	FencedQueue(size_t numFrames)
		: m_data(std::deque<FencedData>(numFrames))
	{}
	virtual ~FencedQueue() = default;

	void FinishFrame(uint64_t fenceValue, uint64_t completedFenceValue) {
		FinishCurrentFrame(fenceValue);
		ReleaseCompletedFrames(completedFenceValue);
	}

protected:
	virtual T ProduceForPush() = 0;
	virtual void FinishCurrentFrame(uint64_t fenceValue) {
		m_data.push(FencedData{ fenceValue, ProduceForPush() });
	}

	virtual void BeforePop(const T& data) {}
	virtual void ReleaseCompletedFrames(uint64_t completedFenceValue) {
		while (!m_data.empty() && m_data.front().fenceValue <= completedFenceValue) {
			BeforePop(m_data.front().data);
			m_data.pop();
		}
	}
};

template <typename T>
class FrameDataBuffer : public FencedQueue<std::vector<T>> {
	size_t m_initCapacity{};
	std::vector<T> m_curr{};

public:
	FrameDataBuffer(size_t numFrames, size_t initCapacity = 16) :
		FencedQueue(numFrames),
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
		m_curr.push_back(std::forward(data));
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
