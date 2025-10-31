#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pD3D12CommandList{};

	std::atomic<bool> m_isReadyForExecution{};

	std::function<void()> m_beforeExec{};
	std::function<void()> m_afterExec{};

public:
	CommandList(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		std::function<void()> beforeExec = []{},
		std::function<void()> afterExec = []{}
	);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList() const;

	bool IsReadyForExection() const;
	void SetReadyForExection();

	void BeforeExecute() const;

	void AfterExecute() const;
};