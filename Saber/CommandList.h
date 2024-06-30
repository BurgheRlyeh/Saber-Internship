#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	uint8_t m_priority{};
	std::function<void(void)> m_beforeExec{};
	std::function<void(void)> m_afterExec{};

public:
	struct Comparator {
		bool operator()(const CommandList& lhs, const CommandList& rhs) {
			return lhs.m_priority < rhs.m_priority;
		}
	};

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pCommandList{};

	CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		uint8_t priority = 0,
		std::function<void(void)> beforeExec = [=]() { return; },
		std::function<void(void)> afterExec = [=]() { return; }
	)
		: m_pCommandList(pCommandList)
		, m_priority(priority)
		, m_beforeExec(beforeExec)
		, m_afterExec(afterExec)
	{}

	bool operator<(const CommandList& other) const {
		return m_priority < other.m_priority;
	}

	uint16_t GetPriority() {
		return m_priority;
	}

	void BeforeExecute() {
		m_beforeExec();
	};
	void AfterExecute() {
		m_afterExec();
	};
};