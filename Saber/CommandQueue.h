#pragma once

#include "Headers.h"

#include <functional>
#include <mutex>
#include <unordered_set>
#include <vector>

#include "CommandListTypes.h"
#include "EnumFence.h"
#include "LockFreeQueue.h"

class CommandAllocatorPool;
class CommandList;
class CommandListPool;
class Device;
template <EnumConcept Enum>
class EnumFence;
class Fence;
class IncrementFence;

class CommandQueue {
	std::wstring m_name;

	Microsoft::WRL::ComPtr<D3D12CommandQueue> m_pCommandQueue{};

	std::shared_ptr<IncrementFence> m_pIncFence{};
	
	std::unique_ptr<CommandAllocatorPool> m_pAllocatorPool{};
	std::unique_ptr<CommandListPool> m_pListPool{};

public:
	CommandQueue() = delete;
	CommandQueue(CommandQueue&&) = delete;
	CommandQueue(const CommandQueue&) = delete;
	CommandQueue(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		CommandQueueType type
	);
	~CommandQueue();

	std::wstring GetName() const {
		return m_name;
	}

	Microsoft::WRL::ComPtr<D3D12CommandQueue> GetD3D12CommandQueue() const;

	D3D12_COMMAND_LIST_TYPE GetCommandListType() const;

	uint64_t ExecuteCommandList(std::shared_ptr<CommandList> commandList);
	void ExecuteCommandListImmediately(std::shared_ptr<CommandList> commandList);

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

	// Get an available command list from the command queue.
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> GetD3D12CommandList(
		std::shared_ptr<Device> pDevice
	);

private:
	// Execute a command list.
	// Returns the fence value to wait for for this command list.
	uint64_t ExecuteD3D12CommandList(
		Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pD3D12CommandList
	);
};
