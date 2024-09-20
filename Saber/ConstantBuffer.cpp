#include "ConstantBuffer.h"

ConstantBuffer::ConstantBuffer(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const UINT64& size,
	void* initData,
	const HeapData& heapData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) : GPUResource(
		pAllocator,
		heapData,
		ResourceData{
			CD3DX12_RESOURCE_DESC::Buffer(size),
			D3D12_RESOURCE_STATE_GENERIC_READ
		},
		allocationFlags
	),
	m_CbvSize((size + 255) & ~255)
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
		.SizeInBytes{ static_cast<UINT>(m_CbvSize) }
	};
	pDevice->CreateConstantBufferView(&cbvDesc, cpuDescHandle);
}

void ConstantBuffer::Update(void* newData) {
	void* pData{};
	ThrowIfFailed(GetResource()->Map(0, &CD3DX12_RANGE(), &pData));
	memcpy(pData, newData, m_CbvSize);
	//GetResource()->Unmap(0, nullptr);
}
