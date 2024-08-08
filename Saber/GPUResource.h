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
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() const;
};