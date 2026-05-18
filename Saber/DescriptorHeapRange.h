#pragma once

#include "Headers.h"

#include <optional>
#include <vector>
#include <mutex>

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

class DescRange {
protected:
	const std::wstring m_name{};

	std::weak_ptr<DescriptorHeap> m_pDescHeapManager{};
	size_t m_startId{};

	size_t m_capacity{};

public:
	DescRange() = delete;
	DescRange(
		const std::wstring& name,
		std::shared_ptr<DescriptorHeap>& pDescHeapManager,
		size_t startId,
		size_t capacity
	);

	virtual ~DescRange() = default;

	DescRange(const DescRange& other) = default;
	DescRange& operator=(const DescRange& other) = default;

	DescRange(DescRange&& other) noexcept = default;
	DescRange& operator=(DescRange&& other) noexcept = default;

	inline const std::wstring& GetName() const { return m_name; }

	virtual size_t GetSize() const = 0;
	inline size_t GetCapacity() const { return m_capacity; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id = 0) const;

	virtual size_t Allocate() = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE AllocateGetCpuHandle();

	virtual void Free(size_t id) = 0;
	void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

	virtual void FreeAll() = 0;
};

class StackDescRange : public DescRange {
	size_t m_size{};

public:
	using DescRange::DescRange;

	size_t GetSize() const override { return m_size; };

	size_t Allocate() override;
	void Free(size_t id) override;
	void FreeAll();
};

class PoolDescRange : public DescRange {
	std::vector<size_t> m_freeIndices{};

public:
	PoolDescRange(
		const std::wstring& name,
		std::shared_ptr<DescriptorHeap>& pDescHeapManager,
		size_t startId,
		size_t capacity
	);

	inline size_t GetSize() const override {
		return m_capacity - m_freeIndices.size();
	};

	size_t Allocate() override;
	void Free(size_t id) override;
	void FreeAll() override;
};
