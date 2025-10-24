#include "ConstantBuffer.h"

ConstantBuffer::ConstantBuffer(
	const std::wstring& name,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const UINT64& size,
	void* initData,
	const HeapData& heapData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) : GPUResource(
		name,
		pAllocator,
		heapData,
		ResourceData{
			CD3DX12_RESOURCE_DESC::Buffer(AlignSize(
				size,
				D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
			)),
			D3D12_RESOURCE_STATE_GENERIC_READ
		},
		allocationFlags
	)
{
	if (initData) {
		Update(initData);
	}
}

void ConstantBuffer::CreateConstantBufferView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle
) {
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
		.BufferLocation{ GetResource()->GetGPUVirtualAddress() },
		.SizeInBytes{ static_cast<UINT>(GetResource()->GetDesc().Width) }
	};
	pDevice->CreateConstantBufferView(&cbvDesc, cpuDescHandle);
}

void ConstantBuffer::Update(void* newData, size_t offset, size_t size) {
	void* pData{};
	ThrowIfFailed(GetResource()->Map(0, &CD3DX12_RANGE(), &pData));
	memcpy(
		static_cast<std::byte*>(pData) + offset,
		newData,
		size ? size : GetResource()->GetDesc().Width
	);
	//GetResource()->Unmap(0, nullptr);
}
