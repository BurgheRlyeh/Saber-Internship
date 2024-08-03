#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

class GPUResource {
protected:
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};

public:
	void CreateResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC& resourceDesc,
		const D3D12_HEAP_TYPE& heapType,
		const D3D12_RESOURCE_STATES& initialResourceState
	) {
		D3D12MA::ALLOCATION_DESC allocationDesc{
			.HeapType{ heapType }
		};

		ThrowIfFailed(pAllocator->CreateResource(
			&allocationDesc,
			&resourceDesc,
			initialResourceState,
			nullptr,
			&m_pAllocation,
			IID_NULL,
			nullptr
		));
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() {
		return m_pAllocation->GetResource();
	}
};