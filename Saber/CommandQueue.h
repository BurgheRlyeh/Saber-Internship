#pragma once

#include "Headers.h"

#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "EnumFence.h"
#include "LockFreeQueue.h"

class CommandList;
class Device;
template <EnumConcept Enum>
class EnumFence;
class Fence;
class IncrementFence;

class CommandQueue {
	std::wstring m_name;

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue{};

	std::shared_ptr<IncrementFence> m_pIncFence{};
	
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

	// Fence
	void Signal(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);
	void CpuWait(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);
	void GpuWait(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);
	void Flush(std::shared_ptr<Fence>& pFence, uint64_t fenceValue);

	// IncrementFence
	uint64_t Signal(std::shared_ptr<IncrementFence>& pFence);
	void CpuWait(std::shared_ptr<IncrementFence>& pFence, uint64_t fenceValue);
	void GpuWait(std::shared_ptr<IncrementFence>& pFence, uint64_t fenceValue);
	void Flush(std::shared_ptr<IncrementFence>& pFence);

	// EnumFence
	template <EnumConcept Enum>
	void Signal(std::shared_ptr<EnumFence<Enum>>& pFence, Enum fenceValue) {
		Signal(std::static_pointer_cast<Fence>(pFence), static_cast<uint64_t>(fenceValue));
	}
	template <EnumConcept Enum>
	void CpuWait(std::shared_ptr<EnumFence<Enum>>& pFence, Enum fenceValue) {
		CpuWait(std::static_pointer_cast<Fence>(pFence), static_cast<uint64_t>(fenceValue));
	}
	template <EnumConcept Enum>
	void GpuWait(std::shared_ptr<EnumFence<Enum>>& pFence, Enum fenceValue) {
		GpuWait(std::static_pointer_cast<Fence>(pFence), static_cast<uint64_t>(fenceValue));
	}
	template <EnumConcept Enum>
	void Flush(std::shared_ptr<EnumFence<Enum>>& pFence) {
		Flush(pFence, std::numeric_limits<std::underlying_type_t<Enum>>::max());
	}

	// CommandQueue's fence
	uint64_t Signal();
	void CpuWait(uint64_t fenceValue);
	void GpuWait(uint64_t fenceValue);
	void Flush();
	bool IsFenceComplete(uint64_t fenceValue);

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
