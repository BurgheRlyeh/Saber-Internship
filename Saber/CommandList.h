#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	uint16_t m_priority{};
	std::function<void(void)> m_beforeExec{};
	std::function<void(void)> m_afterExec{};

public:
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pCommandList{};

	CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		uint16_t priority = 0,
		std::function<void(void)> beforeExec = [=]() { return; },
		std::function<void(void)> afterExec = [=]() { return; }
	)
		: m_pCommandList(pCommandList)
		, m_priority(priority)
		, m_beforeExec(beforeExec)
		, m_afterExec(afterExec)
	{}

	void BeforeExecute() {
		m_beforeExec();
	};
	void AfterExecute() {
		m_afterExec();
	};
};