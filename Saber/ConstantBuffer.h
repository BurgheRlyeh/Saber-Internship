#pragma once

#include "Headers.h"

#include "GPUResource.h"

class ConstantBuffer : public GPUResource {
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pCBV{};
	size_t m_CBVSizeInBytes{};
	bool m_isEmpty{ true };

public:
	ConstantBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
		void* initData = nullptr,
		const D3D12_RESOURCE_FLAGS& resFlags = D3D12_RESOURCE_FLAG_NONE,
		const D3D12_HEAP_TYPE& heapType = D3D12_HEAP_TYPE_UPLOAD,
		bool createCBV = false
	);

	void CreateCBV(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice);

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetCBV() const;

	void Update(void* newData);

	bool IsEmpty() const;
};
