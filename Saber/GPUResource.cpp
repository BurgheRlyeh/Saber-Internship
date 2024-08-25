#include "GPUResource.h"

GPUResource::GPUResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const D3D12_RESOURCE_DESC& resourceDesc,
	const D3D12_HEAP_TYPE& heapType,
	const D3D12_RESOURCE_STATES& initialResourceState,
	const D3D12_CLEAR_VALUE* pClearValue,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	CreateResource(pAllocator, resourceDesc, heapType, initialResourceState, pClearValue, allocationFlags);
}

void GPUResource::CreateResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const D3D12_RESOURCE_DESC& resourceDesc,
	const D3D12_HEAP_TYPE& heapType,
	const D3D12_RESOURCE_STATES& initialResourceState,
	const D3D12_CLEAR_VALUE* pClearValue,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	D3D12MA::ALLOCATION_DESC allocationDesc{
		.Flags{ allocationFlags },
		.HeapType{ heapType }
	};

	ThrowIfFailed(pAllocator->CreateResource(
		&allocationDesc,
		&resourceDesc,
		initialResourceState,
		pClearValue,
		&m_pAllocation,
		IID_NULL,
		nullptr
	));
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetResource() const {
	return m_pAllocation->GetResource();
}
