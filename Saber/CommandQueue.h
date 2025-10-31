#pragma once

#include "Headers.h"

// To avoid conflicts and use only min/max defined in <algorithm>
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#include <queue>
#include <vector>
#include <unordered_set>
#include <mutex>

#include "CommandList.h"
#include "LockFreeQueue.h"

class CommandQueue {
	static const std::wstring BASE_NAME;

	struct CommandAllocatorEntry {
		uint64_t fenceValue{};
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};
	};

	D3D12_COMMAND_LIST_TYPE m_commandListType{ D3D12_COMMAND_LIST_TYPE_NONE };
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue{};
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{};
	HANDLE m_fenceEvent{};
	uint64_t m_fenceValue{};

	ArrayLockFreeQueue<CommandAllocatorEntry> m_commandAllocatorQueue{};
	ArrayLockFreeQueue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListQueue{};

	struct PrioritySet {
		std::mutex mutex{};
		std::unordered_multiset<std::shared_ptr<CommandList>> pCommandLists{};
	};
	std::mutex m_commandListsSetsMutex{};
	std::vector<std::unique_ptr<PrioritySet>> m_commandListSets{};

public:
	CommandQueue() = delete;
	CommandQueue(CommandQueue&&) = delete;
	CommandQueue(const CommandQueue&) = delete;
	CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice, D3D12_COMMAND_LIST_TYPE type);
	~CommandQueue();

	D3D12_COMMAND_LIST_TYPE GetCommandListType() const;

	std::shared_ptr<CommandList> GetCommandList(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
	);
	std::shared_ptr<CommandList> GetDeferredCommandList(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		size_t priority,
		std::function<void()> beforeExecuteTask = []{},
		std::function<void()> afterExecuteTask = []{}
	);

	uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
	void ExecuteCommandListImmediately(std::shared_ptr<CommandList> commandList);

	void PushForExecution(std::shared_ptr<CommandList> pCommandList);
	uint64_t ExecutionTask(uint64_t waitFenceValue);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

private:
	// Get an available command list from the command queue.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
	);

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteD3D12CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pD3D12CommandList
	);

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice
	);
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator
	);
};
