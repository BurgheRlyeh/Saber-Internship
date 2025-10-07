#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pCommandList{};

	uint8_t m_priority{};
	std::function<void()> m_beforeExec{};
	std::function<void()> m_afterExec{};
	std::atomic<bool> m_isReadyForExecution{};

public:
	CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		uint8_t priority = 0,
		std::function<void()> beforeExec = []{},
		std::function<void()> afterExec = []{}
	);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList() const;

	uint16_t GetPriority() const;

	bool IsReadyForExection() const;

	void SetReadyForExection();

	void BeforeExecute() const;

	void AfterExecute() const;
};