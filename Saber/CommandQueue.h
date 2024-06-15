#pragma once

#include "Headers.h"

#include <queue>

class CommandQueue {
	struct CommandAllocatorEntry {
		uint64_t fenceValue{};
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};
	};

	D3D12_COMMAND_LIST_TYPE m_commandListType{ D3D12_COMMAND_LIST_TYPE_NONE };
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue{};
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{};
	HANDLE m_fenceEvent{};
	uint64_t m_fenceValue{};

	std::queue<CommandAllocatorEntry> m_commandAllocatorQueue{};
	std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListQueue{};

public:
	CommandQueue() = delete;
	CommandQueue(CommandQueue&&) = delete;
	CommandQueue(const CommandQueue&) = delete;
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue();

	D3D12_COMMAND_LIST_TYPE GetCommandListType() const;

	// Get an available command list from the command queue.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice);;

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList);
	void ExecuteCommandListImmediately(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice);
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator
	);
};
