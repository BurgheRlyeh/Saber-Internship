#pragma once

#include "Headers.h"

#include "CommandListTypes.h"

#include <functional>
#include <memory>

class CommandListManager;

class CommandList : public std::enable_shared_from_this<CommandList> {
	std::wstring m_name{};

	CommandListManager& m_manager;

	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> m_pD3D12CommandList{};

	size_t m_priority{};

	std::function<void()> m_beforeExec{};
	std::function<void()> m_afterExec{};

	uint8_t m_pixEventsBegan{};

public:
	CommandList(
		const std::wstring& name,
		CommandListManager& manager,
		Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pCommandList,
		size_t priority = 0,
		std::function<void()> beforeExec = []{},
		std::function<void()> afterExec = []{}
	);

	const std::wstring& GetName() const { return m_name; }
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> GetD3D12CommandList() const;
	CommandListType GetType() const;

	size_t GetPriority() const { return m_priority; }

	uint64_t Execute();
	void ExecuteImmediately();
	void PushForExecution();

	void BeforeExecute() const;
	void AfterExecute() const;

	void PixBeginEvent(const std::wstring& name, uint8_t r = 0, uint8_t g = 0, uint8_t b = 0);
	void PixEndEvent();
	void PixEndAllEvents();
};
