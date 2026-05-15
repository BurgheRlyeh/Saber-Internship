#pragma once

#include "Headers.h"

#include <optional>

#include "Atlas.h"

class DescRange;
class Device;

class DescriptorHeapManager {
	D3D12_DESCRIPTOR_HEAP_DESC m_heapDesc{};
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescHeap{};
	UINT m_handleIncSize{};

	std::shared_ptr<Atlas<DescRange>> m_pRangesAtlas{};

	size_t m_firstFreeId{};

public:
	DescriptorHeapManager(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
	);

	DescriptorHeapManager(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const D3D12_DESCRIPTOR_HEAP_TYPE& type,
		const size_t& size,
		const D3D12_DESCRIPTOR_HEAP_FLAGS& flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE
	) : DescriptorHeapManager(
			name,
			pDevice,
			D3D12_DESCRIPTOR_HEAP_DESC{ type, static_cast<UINT>(size), flags }
		)
	{}

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() const;

	std::shared_ptr<DescRange> AllocateRange(
		const std::wstring& name,
		const size_t& size,
		const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type = std::nullopt
	);

	std::shared_ptr<DescRange> GetRange(const std::wstring& name);
};
