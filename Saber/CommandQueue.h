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
	std::mutex m_commandAllocatorQueueMutex{};
	std::queue<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2>> m_commandListQueue{};
	std::mutex m_commandListQueueMutex{};

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

	// Get an available command list from the command queue.
	std::shared_ptr<CommandList> GetCommandList(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		bool isDeffered = false,
		uint8_t priority = 0,
		std::function<void(void)> beforeExecuteTask = [=]() { return; },
		std::function<void(void)> afterExecuteTask = [=]() { return; }
	);

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
	void ExecuteCommandListImmediately(std::shared_ptr<CommandList> commandList);

	void PushForExecution(std::shared_ptr<CommandList> pCommandList);
	uint64_t ExecutionTask();

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
