#pragma once

#include "Headers.h"

#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "LockFreeQueue.h"

class CommandList;
class Device;

class CommandQueue {
	std::wstring m_name;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue{};

	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence{};
	uint64_t m_fenceValue{};
	HANDLE m_fenceEvent{};
	
	struct CommandAllocatorEntry {
		uint64_t fenceValue{};
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pCommandAllocator{};
	};
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
	CommandQueue(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		D3D12_COMMAND_LIST_TYPE type
	);
	~CommandQueue();

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;

	D3D12_COMMAND_LIST_TYPE GetCommandListType() const;

	std::shared_ptr<CommandList> GetCommandList(
		std::shared_ptr<Device> pDevice
	);
	std::shared_ptr<CommandList> GetDeferredCommandList(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		size_t priority,
		std::function<void()> beforeExecuteTask = []{},
		std::function<void()> afterExecuteTask = []{}
	);

	uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
	void ExecuteCommandListImmediately(std::shared_ptr<CommandList> commandList);

	void PushForExecution(std::shared_ptr<CommandList> pCommandList);
	uint64_t ExecutionTask(uint64_t waitFenceValue);

	uint64_t Signal();
	void Wait(uint64_t fenceValue);
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();

private:
	static Microsoft::WRL::ComPtr<ID3D12CommandQueue> CreateCommandQueue(
		std::shared_ptr<Device> pDevice,
		D3D12_COMMAND_LIST_TYPE type
	);

	// Get an available command list from the command queue.
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetD3D12CommandList(
		std::shared_ptr<Device> pDevice
	);

	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteD3D12CommandList(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pD3D12CommandList
	);

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(
		std::shared_ptr<Device> pDevice
	) const;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(
		std::shared_ptr<Device> pDevice,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> pAllocator
	) const;
};
