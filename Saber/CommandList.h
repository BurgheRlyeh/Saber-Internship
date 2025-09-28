#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	uint8_t m_priority{};
	std::function<void(void)> m_beforeExec{};
	std::function<void(void)> m_afterExec{};
	std::atomic<bool> m_isReadyForExecution{};

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pCommandList{};

public:
	CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		uint8_t priority = 0,
		std::function<void(void)> beforeExec = [=]() { return; },
		std::function<void(void)> afterExec = [=]() { return; }
	);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList() const {
		return m_pCommandList;
	}

	uint16_t GetPriority() const;

	bool IsReadyForExection() const;

	void SetReadyForExection();

	void BeforeExecute() const;

	void AfterExecute() const;
};