#include "CommandAllocatorPool.h"

#include "Device.h"
#include "IncrementFence.h"

namespace {
	Microsoft::WRL::ComPtr<D3D12CommandAllocator> CreateD3D12CommandAllocator(
		std::shared_ptr<Device>& pDevice,
		D3D12_COMMAND_LIST_TYPE type
	) {
		Microsoft::WRL::ComPtr<D3D12CommandAllocator> pCommandAllocator{};
		ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandAllocator(
			type,
			IID_PPV_ARGS(&pCommandAllocator)
		));

		return pCommandAllocator;
	}
}

Microsoft::WRL::ComPtr<D3D12CommandAllocator> CommandAllocatorPool::Request(
	std::shared_ptr<Device>& pDevice,
	std::shared_ptr<IncrementFence> pFence
) {
	Microsoft::WRL::ComPtr<D3D12CommandAllocator> pCommandAllocator{};
	
	std::unique_lock<std::mutex> poolLock(m_mutex);
	if (!m_pool.empty() && pFence->IsCompleted(m_pool.front().fenceValue)) {
		pCommandAllocator = m_pool.front().pCommandAllocator;
		m_pool.pop();
		poolLock.unlock();

		pCommandAllocator->Reset();
	}
	else {
		poolLock.unlock();
		pCommandAllocator = CreateD3D12CommandAllocator(pDevice, ToD3D12Type(GetType()));
	}

	return pCommandAllocator;
}

void CommandAllocatorPool::Discard(
	Microsoft::WRL::ComPtr<D3D12CommandAllocator> pAllocator,
	uint64_t fenceValue
) {
	std::scoped_lock<std::mutex> lock(m_mutex);
	m_pool.push(CommandAllocatorEntry{ pAllocator, fenceValue });
}
