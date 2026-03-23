#pragma once

#include "Headers.h"

#include <functional>

class CommandList {
	std::wstring m_name{};

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> m_pD3D12CommandList{};

	std::atomic<bool> m_isReadyForExecution{};

	std::function<void()> m_beforeExec{};
	std::function<void()> m_afterExec{};

	uint8_t m_pixEventsBegan{};

public:
	CommandList(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		std::function<void()> beforeExec = []{},
		std::function<void()> afterExec = []{}
	);

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList() const;
	D3D12_COMMAND_LIST_TYPE GetType() const;

	bool IsReadyForExecution() const;
	void SetReadyForExecution();

	void BeforeExecute() const;
	void AfterExecute() const;

	void PixBeginEvent(const std::wstring& name, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
	void PixEndEvent();
};
