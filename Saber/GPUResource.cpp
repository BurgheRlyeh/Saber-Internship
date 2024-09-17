#include "GPUResource.h"

GPUResource::GPUResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	CreateResource(pAllocator, heapData, resData, allocationFlags);
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetResource() const {
	return m_pAllocation->GetResource();
}

void GPUResource::CreateResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	D3D12MA::ALLOCATION_DESC allocationDesc{
		.Flags{ allocationFlags },
		.HeapType{ heapData.heapType },
		.ExtraHeapFlags{ heapData.heapFlags }
	};

	ThrowIfFailed(pAllocator->CreateResource(
		&allocationDesc,
		&resData.resDesc,
		resData.resInitState,
		resData.pResClearValue,
		&m_pAllocation,
		IID_NULL,
		nullptr
	));
}
