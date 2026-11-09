#pragma once

#include "Headers.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <mutex>

#include "IndexRange.h"

class DescriptorHeap;

enum class DescRangeType : uint8_t {
	Cbv = 0,
	Srv = 1,
	Uav = 2,
	Rtv = 3,
	Dsv = 4,
	Smp = 5,
	NumTypes = 6,

	// TODO: add smth like "BufferStartType = Cbv", and same for textures
	ResNumTypes = 5
};

constexpr D3D12_DESCRIPTOR_HEAP_TYPE ToD3D12DescHeapType(DescRangeType type) {
	switch (type) {
	case DescRangeType::Cbv:
	case DescRangeType::Srv:
	case DescRangeType::Uav:
		return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	case DescRangeType::Rtv:
		return D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	case DescRangeType::Dsv:
		return D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	case DescRangeType::Smp:
		return D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	default:
		return D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	}
}

constexpr std::wstring ToName(DescRangeType type) {
	switch (type) {
	case DescRangeType::Cbv: return L"Cbv";
	case DescRangeType::Srv: return L"Srv";
	case DescRangeType::Uav: return L"Uav";
	case DescRangeType::Rtv: return L"Rtv";
	case DescRangeType::Dsv: return L"Dsv";
	case DescRangeType::Smp: return L"Smp";
	default: return L"";
	}
}

// Descriptors of a heap from startId on, handed out by its index range
class DescRange {
	const std::wstring m_name{};

	// Kept alive by its ranges: one can't outlive the heap it gives descriptors back to
	std::shared_ptr<DescriptorHeap> m_pDescHeap{};
	size_t m_startId{};

	std::unique_ptr<IndexRange> m_pIds{};

public:
	DescRange() = delete;

	// The tag picks how ids are handed out: a constructor can't take template arguments explicitly
	template <IndexRangeConcept IndexRangeImpl>
	DescRange(
		const std::wstring& name,
		const std::shared_ptr<DescriptorHeap>& pDescHeap,
		size_t startId,
		size_t capacity,
		std::in_place_type_t<IndexRangeImpl>
	) : DescRange(name, pDescHeap, startId, std::make_unique<IndexRangeImpl>(capacity))
	{}

private:
	DescRange(
		const std::wstring& name,
		const std::shared_ptr<DescriptorHeap>& pDescHeap,
		size_t startId,
		std::unique_ptr<IndexRange> pIds
	);

public:
	// Gives its descriptors back to the heap
	~DescRange();

	DescRange(const DescRange&) = delete;
	DescRange& operator=(const DescRange&) = delete;

	DescRange(DescRange&&) = delete;
	DescRange& operator=(DescRange&&) = delete;

	inline const std::wstring& GetName() const { return m_name; }

	inline size_t GetSize() const { return m_pIds->GetSize(); }
	inline size_t GetCapacity() const { return m_pIds->GetCapacity(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id = 0) const;

	inline size_t Allocate() { return m_pIds->Allocate(); }
	D3D12_CPU_DESCRIPTOR_HANDLE AllocateGetCpuHandle();

	inline void Free(size_t id) { m_pIds->Free(id); }
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

	inline void FreeAll() { m_pIds->FreeAll(); }
};
