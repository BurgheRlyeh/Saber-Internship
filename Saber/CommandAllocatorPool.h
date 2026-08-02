#pragma once

#include "Headers.h"

#include <mutex>
#include <queue>

#include "CommandListTypes.h"

class Device;
class CommandQueue;
class IncrementFence;

class CommandAllocatorPool {
	CommandListType m_type{ CommandListType::None };

	struct CommandAllocatorEntry {
		Microsoft::WRL::ComPtr<D3D12CommandAllocator> pCommandAllocator{};
		uint64_t fenceValue{};
	};
	std::queue<CommandAllocatorEntry> m_pool{};
	std::mutex m_mutex{};

public:
	CommandAllocatorPool(CommandListType type) : m_type(type) {}

	CommandListType GetType() const { return m_type; }

	Microsoft::WRL::ComPtr<D3D12CommandAllocator> Request(
		std::shared_ptr<Device>& pDevice,
		std::shared_ptr<IncrementFence> pFence
	);

	void Discard(Microsoft::WRL::ComPtr<D3D12CommandAllocator> pAllocator, uint64_t fenceValue);
};
