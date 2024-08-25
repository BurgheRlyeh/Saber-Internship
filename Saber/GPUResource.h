#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

class GPUResource {
protected:
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};

public:
	static void ResourceTransition(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
		D3D12_RESOURCE_STATES stateBefore,
		D3D12_RESOURCE_STATES stateAfter
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

	GPUResource() = default;
	GPUResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC& resourceDesc,
		const D3D12_HEAP_TYPE& heapType,
		const D3D12_RESOURCE_STATES& initialResourceState,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	void CreateResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_DESC& resourceDesc,
		const D3D12_HEAP_TYPE& heapType,
		const D3D12_RESOURCE_STATES& initialResourceState,
		const D3D12_CLEAR_VALUE* pClearValue = nullptr,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() const;
};