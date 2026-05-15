#pragma once

#include "Headers.h"

#include <optional>

enum class DescRangeType : uint8_t {
	Srv = 0,
	Uav = 1,
	Cbv = 2,
	Rtv = 3,
	Dsv = 4,
	Smp = 5,
	NumTypes = 6,

	ResNumTypes = 5
};

constexpr std::wstring ToName(DescRangeType type) {
	switch (type) {
	case DescRangeType::Srv: return L"Srv";
	case DescRangeType::Uav: return L"Uav";
	case DescRangeType::Cbv: return L"Cbv";
	case DescRangeType::Rtv: return L"Rtv";
	case DescRangeType::Dsv: return L"Dsv";
	case DescRangeType::Smp: return L"Smp";
	default: return L"";
	}
}	

class DescRange {
	const std::wstring m_name{};

	UINT m_handleIncSize{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
	std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> m_type{};

	size_t m_size{};
	size_t m_capacity{};

public:
	DescRange() = delete;
	DescRange(const DescRange& other) = default;

	DescRange(
		const std::wstring& name,
		const size_t& capacity,
		const UINT& handleIncSize,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
		const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle,
		const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type = std::nullopt
	);
	DescRange(
		const std::wstring& name,
		const DescRange& other
	);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id = 0) const;

	size_t GetSize() const;

	size_t GetNextId();
	D3D12_CPU_DESCRIPTOR_HANDLE GetNextCpuHandle();

	void Clear();
};

