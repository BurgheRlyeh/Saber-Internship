#pragma once

#include "Headers.h"

#include <optional>

#include "Atlas.h"
#include "DescriptorHeapRange.h"

class DescriptorHeapManager {
	D3D12_DESCRIPTOR_HEAP_DESC m_heapDesc{};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescHeap{};
	UINT m_handleIncSize{};

	std::shared_ptr<Atlas<DescHeapRange>> m_pRangesAtlas{};

	size_t m_firstFreeId{};

public:
	DescriptorHeapManager(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
	) : m_heapDesc(heapDesc) {
		m_pRangesAtlas = std::make_shared<Atlas<DescHeapRange>>(name);

		ThrowIfFailed(pDevice->CreateDescriptorHeap(&m_heapDesc, IID_PPV_ARGS(&m_pDescHeap)));
		m_pDescHeap->SetName(name.c_str());

		m_handleIncSize = pDevice->GetDescriptorHandleIncrementSize(m_heapDesc.Type);
	}

	DescriptorHeapManager(
		const std::wstring& name,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_DESCRIPTOR_HEAP_TYPE& type,
		const size_t& size,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	) : DescriptorHeapManager(
			name,
			pDevice,
			D3D12_DESCRIPTOR_HEAP_DESC{ type, static_cast<UINT>(size), flags }
		)
	{}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() const {
		return m_pDescHeap;
	}

	std::shared_ptr<DescHeapRange> AllocateRange(
		const std::wstring& name,
		const size_t& size,
		const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type = std::nullopt
	) {
		assert(size);
		assert(m_firstFreeId + size <= m_heapDesc.NumDescriptors);

		std::shared_ptr<DescHeapRange> pRange{ m_pRangesAtlas->Assign(
			name,
			size,
			m_handleIncSize,
			CD3DX12_CPU_DESCRIPTOR_HANDLE(
				m_pDescHeap->GetCPUDescriptorHandleForHeapStart(),
				static_cast<UINT>(m_firstFreeId),
				m_handleIncSize
			),
			CD3DX12_GPU_DESCRIPTOR_HANDLE(
				m_pDescHeap->GetGPUDescriptorHandleForHeapStart(),
				static_cast<UINT>(m_firstFreeId),
				m_handleIncSize
			),
			type
		) };
		m_firstFreeId += size;

		return pRange;
	}

	std::shared_ptr<DescHeapRange> GetRange(const std::wstring& name) {
		return m_pRangesAtlas->Find(name);
	}
};
