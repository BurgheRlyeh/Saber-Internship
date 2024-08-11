#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	uint8_t m_priority{};
	std::function<void(void)> m_beforeExec{};
	std::function<void(void)> m_afterExec{};
	std::atomic<bool> m_isReadyForExecution{};

public:
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pCommandList{};

	CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		uint8_t priority = 0,
		std::function<void(void)> beforeExec = [=]() { return; },
		std::function<void(void)> afterExec = [=]() { return; }
	);

	template <typename T>
	inline T operator->() const {
		return m_pCommandList;
	}

	inline operator Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>() const {
		return m_pCommandList;
	}

	uint16_t GetPriority() const;

	bool IsReadyForExection() const;

	void SetReadyForExection();

	void BeforeExecute() const;

	void AfterExecute() const;
};