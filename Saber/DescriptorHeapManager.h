#pragma once

#include "Headers.h"

#include <mutex>

#include <array>
#include <memory>

#include "Atlas.h"
#include "DescriptorHeapRange.h"

class DescRange;
class Device;

class DescriptorHeap : public std::enable_shared_from_this<DescriptorHeap> {
	friend class DescRange;

	D3D12_DESCRIPTOR_HEAP_DESC m_heapDesc{};
	Microsoft::WRL::ComPtr<D3D12DescriptorHeap> m_pDescHeap{};
	UINT m_handleIncSize{};

	std::shared_ptr<Atlas<DescRange>> m_pRangesAtlas{};

	size_t m_firstFreeId{};

	// Ranges are allocated from job system threads while scenes load
	std::mutex m_mutex{};

public:
	DescriptorHeap(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
	);

	inline Microsoft::WRL::ComPtr<D3D12DescriptorHeap> GetD3D12DescriptorHeap() const {
		return m_pDescHeap;
	}

	template <std::derived_from<DescRange> DescRangeImpl = StackDescRange>
	std::shared_ptr<DescRange> AllocateRange(const std::wstring& basename, DescRangeType descRangeType, size_t size) {
		assert(size);
		assert(m_heapDesc.Type == ToD3D12DescHeapType(descRangeType));

		std::scoped_lock<std::mutex> lock(m_mutex);
		assert(m_firstFreeId + size <= m_heapDesc.NumDescriptors);

		std::shared_ptr<DescRange> pRange{ m_pRangesAtlas->Assign<DescRangeImpl>(
			basename + L"/Ranges/" + ToName(descRangeType),
			this->shared_from_this(),
			m_firstFreeId,
			size
		) };
		m_firstFreeId += size;

		return pRange;
	}

private:
	inline UINT GetHandleIncrementSize() const { return m_handleIncSize; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id) const;
};

class DescriptorHeapManager {
	std::wstring m_name{};

	std::array<
		std::shared_ptr<DescriptorHeap>,
		D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES
	> m_pDescHeaps{};

public:
	struct DescHeapArgs {
		size_t size{};
		D3D12_DESCRIPTOR_HEAP_FLAGS flags{};
		uint32_t nodeMask{};
	};
	DescriptorHeapManager(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const std::array<DescHeapArgs, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& descHeapArgs
	);

	std::shared_ptr<DescriptorHeap> GetDescHeap(DescRangeType descRangeType) const {
		D3D12_DESCRIPTOR_HEAP_TYPE heapType{ ToD3D12DescHeapType(descRangeType) };
		assert(heapType < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES);
		return m_pDescHeaps[heapType];
	}

	template <std::derived_from<DescRange> DescRangeImpl = StackDescRange>
	std::shared_ptr<DescRange> AllocateRange(const std::wstring& basename, DescRangeType descRangeType, size_t size) {
		return GetDescHeap(descRangeType)->AllocateRange<DescRangeImpl>(basename, descRangeType, size);
	}
};
