#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

class ConstantBuffer {
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};
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
	) : m_CBVSizeInBytes((resAllocInfo.SizeInBytes + 255) & ~255) {
		D3D12_RESOURCE_DESC resDesc{ CD3DX12_RESOURCE_DESC::Buffer(resAllocInfo, resFlags) };

		D3D12MA::ALLOCATION_DESC allocDesc{
			.HeapType{ heapType }
		};

		ThrowIfFailed(pAllocator->CreateResource(
			&allocDesc,
			&resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			&m_pAllocation,
			IID_NULL,
			nullptr
		));

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

	void CreateCBV(Microsoft::WRL::ComPtr<ID3D12Device2> pDevice) {
		if (m_pCBV) {
			return;
		}

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{
			.BufferLocation{ GetResource()->GetGPUVirtualAddress() },
			.SizeInBytes{ static_cast<UINT>(m_CBVSizeInBytes) }
		};
		pDevice->CreateConstantBufferView(&cbvDesc, m_pCBV->GetCPUDescriptorHandleForHeapStart());
	}

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() {
		return m_pAllocation->GetResource();
	}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetCBV() {
		return m_pCBV;
	}

	void Update(void* newData) {
		void* pData{};
		ThrowIfFailed(GetResource()->Map(0, &CD3DX12_RANGE(), &pData));
		memcpy(pData, newData, m_CBVSizeInBytes);
		m_isEmpty = false;
	}

	bool IsEmpty() {
		return m_isEmpty;
	}
};
