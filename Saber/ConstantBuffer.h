#pragma once

#include "Headers.h"

#include "GPUResource.h"

class ConstantBuffer : public GPUResource {
	size_t m_CbvSize{};

public:
	ConstantBuffer(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const UINT64& size,
		void* initData = nullptr,
		const HeapData& heapData = HeapData{ D3D12_HEAP_TYPE_UPLOAD },
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	void CreateConstantBufferView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle
	);

	void Update(void* newData);
};
