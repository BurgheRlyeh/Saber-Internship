#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

class GPUResource {
protected:
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};
	GPUResource() = default;

public:
	struct HeapData {
		D3D12_HEAP_TYPE heapType{};
		D3D12_HEAP_FLAGS heapFlags{};
	};
	struct ResourceData {
		D3D12_RESOURCE_DESC resDesc{};
		D3D12_RESOURCE_STATES resInitState{};
		const D3D12_CLEAR_VALUE* pResClearValue{};
	};
	GPUResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() const;

protected:
	void CreateResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);
};

static void ResourceTransition(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
	const D3D12_RESOURCE_STATES& stateBefore,
	const D3D12_RESOURCE_STATES& stateAfter
) {
	pCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pResource.Get(),
			stateBefore,
			stateAfter
		)
	);
}
