#pragma once

#include "Headers.h"

#include "CommandListTypes.h"
#include "LockFreeQueue.h"

class Device;

class CommandListPool {
	static constexpr size_t DefaultCommandListPoolSize{ 16 };

	CommandListType m_type{ CommandListType::None };

	ArrayLockFreeQueue<Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>> m_pool{ DefaultCommandListPoolSize };

public:
	CommandListPool(CommandListType type) : m_type(type) {}

	CommandListType GetType() const { return m_type; }

	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> Request(
		std::shared_ptr<Device>& pDevice,
		Microsoft::WRL::ComPtr<D3D12CommandAllocator> pAllocator
	);

	void Discard(Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>& pCommandList);
};
