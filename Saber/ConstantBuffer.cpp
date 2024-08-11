#include "ConstantBuffer.h"

ConstantBuffer::ConstantBuffer(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
	void* initData,
	const D3D12_RESOURCE_FLAGS& resFlags,
	const D3D12_HEAP_TYPE& heapType,
	bool createCBV
) : m_CBVSizeInBytes((resAllocInfo.SizeInBytes + 255) & ~255)
{
	CreateResource(
		pAllocator,
		CD3DX12_RESOURCE_DESC::Buffer(resAllocInfo, resFlags),
		heapType,
		D3D12_RESOURCE_STATE_GENERIC_READ
	);

	if (createCBV) {
		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
			.BufferLocation{ GetResource()->GetGPUVirtualAddress() },
			.SizeInBytes{ static_cast<UINT>(m_CBVSizeInBytes) }
		};
		pDevice->CreateConstantBufferView(&cbvDesc, m_pCBV->GetCPUDescriptorHandleForHeapStart());
	}

	if (initData) {
		/* TODO: thread-safety
		* In common scenarios,
		* the application merely must write to memory before calling ExecuteCommandLists;
		* but using a fence to delay command list execution works as well.
		*/
		CD3DX12_RANGE readRange{};
		void* pData{};
		ThrowIfFailed(GetResource()->Map(0, &readRange, &pData));
		memcpy(pData, initData, m_CBVSizeInBytes);
		m_isEmpty = false;
	}
}

void ConstantBuffer::CreateCBV(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice) {
	if (m_pCBV) {
		return;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
		.BufferLocation{ GetResource()->GetGPUVirtualAddress() },
		.SizeInBytes{ static_cast<UINT>(m_CBVSizeInBytes) }
	};
	pDevice->CreateConstantBufferView(&cbvDesc, m_pCBV->GetCPUDescriptorHandleForHeapStart());
}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> ConstantBuffer::GetCBV() const {
	return m_pCBV;
}

void ConstantBuffer::Update(void* newData) {
	void* pData{};
	ThrowIfFailed(GetResource()->Map(0, &CD3DX12_RANGE(), &pData));
	memcpy(pData, newData, m_CBVSizeInBytes);
	if (IsEmpty())
		m_isEmpty = false;
}

bool ConstantBuffer::IsEmpty() const {
	return m_isEmpty;
}