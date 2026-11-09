#pragma once

#include "Headers.h"

#include <mutex>

#include <array>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "Atlas.h"
#include "DescriptorHeapRange.h"
#include "FencedQueue.h"
#include "IndexRange.h"

class DescRange;
class Device;

// Ranges released during a frame, given back to the allocator once the frame completes
// Stored as pairs of (first id, count)
class FrameReleasedRanges : public FrameDataBuffer<std::pair<size_t, size_t>> {
	RangeAllocator& m_allocator;

public:
	explicit FrameReleasedRanges(RangeAllocator& allocator)
		: FrameDataBuffer(0), m_allocator(allocator)
	{}

protected:
	void BeforePop(const std::vector<std::pair<size_t, size_t>>& ranges) override {
		for (const auto& [startId, size] : ranges) {
			m_allocator.Free(startId, size);
		}
	}
};

class DescriptorHeap : public std::enable_shared_from_this<DescriptorHeap> {
	friend class DescRange;

	D3D12_DESCRIPTOR_HEAP_DESC m_heapDesc{};
	Microsoft::WRL::ComPtr<D3D12DescriptorHeap> m_pDescHeap{};
	UINT m_handleIncSize{};

	std::shared_ptr<Atlas<DescRange>> m_pRangesAtlas{};

	RangeAllocator m_allocator;
	FrameReleasedRanges m_releasedRanges;	// shader-visible only

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

	template <IndexRangeConcept IndexRangeImpl = StackIndexRange>
	std::shared_ptr<DescRange> AllocateRange(const std::wstring& basename, DescRangeType descRangeType, size_t size) {
		assert(size);
		assert(m_heapDesc.Type == ToD3D12DescHeapType(descRangeType));

		const std::wstring name{ basename + L"/Ranges/" + ToName(descRangeType) };

		std::scoped_lock<std::mutex> lock(m_mutex);

		// The same name gives the same range
		if (std::shared_ptr<DescRange> pExisting{ m_pRangesAtlas->Find(name) }) {
			assert(pExisting->GetCapacity() >= size);
			return pExisting;
		}

		const std::optional<size_t> startId{ m_allocator.Allocate(size) };
		assert(startId && "Descriptor heap is full or too fragmented");

		return m_pRangesAtlas->Assign(
			name,
			this->shared_from_this(),
			*startId,
			size,
			std::in_place_type<IndexRangeImpl>
		);
	}

	// Ranges released before this frame's end are reused once it completes
	void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue);

private:
	// From a destroyed range
	void Release(size_t startId, size_t size);

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

	template <IndexRangeConcept IndexRangeImpl = StackIndexRange>
	std::shared_ptr<DescRange> AllocateRange(const std::wstring& basename, DescRangeType descRangeType, size_t size) {
		return GetDescHeap(descRangeType)->AllocateRange<IndexRangeImpl>(basename, descRangeType, size);
	}

	void FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue);
};
