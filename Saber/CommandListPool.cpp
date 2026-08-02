#include "CommandListPool.h"

#include "Device.h"

namespace {
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> CreateCommandList(
		std::shared_ptr<Device> pDevice,
		D3D12_COMMAND_LIST_TYPE type,
		Microsoft::WRL::ComPtr<D3D12CommandAllocator> pAllocator
	) {
		Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pCommandList{};
		ThrowIfFailed(pDevice->GetD3D12Device()->CreateCommandList(
			0,
			type,
			pAllocator.Get(),
			nullptr,
			IID_PPV_ARGS(&pCommandList)
		));

		return pCommandList;
	}
}

Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> CommandListPool::Request(
	std::shared_ptr<Device>& pDevice,
	Microsoft::WRL::ComPtr<D3D12CommandAllocator> pAllocator
) {
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList> pCommandList{};

	if (m_pool.Dequeue(pCommandList)) {
		ThrowIfFailed(pCommandList->Reset(pAllocator.Get(), nullptr));
	}
	else {
		pCommandList = CreateCommandList(pDevice, ToD3D12Type(GetType()), pAllocator);
	}

	// Associate the command allocator with the command list so that it can be
	// retrieved when the command list is executed.
	ThrowIfFailed(pCommandList->SetPrivateDataInterface(
		__uuidof(D3D12CommandAllocator),
		pAllocator.Get()
	));

	return pCommandList;
}

void CommandListPool::Discard(
	Microsoft::WRL::ComPtr<D3D12GraphicsCommandList>& pCommandList
) {
	bool result{ m_pool.Enqueue(pCommandList) };
	assert(result);
}
