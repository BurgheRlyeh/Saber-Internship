#pragma once

#include "Headers.h"

#include <optional>

class DescHeapRange {
	const std::wstring& m_name{};

	UINT m_handleIncSize{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
	std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> m_type{};

	size_t m_size{};
	size_t m_capacity{};

public:
	DescHeapRange() = delete;
	DescHeapRange(const DescHeapRange& other) = default;

	DescHeapRange(
		const std::wstring& name,
		const size_t& capacity,
		const UINT& handleIncSize,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
		const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle,
		const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type = std::nullopt
	);
	DescHeapRange(
		const std::wstring& name,
		const DescHeapRange& other
	);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id = 0) const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id = 0) const;

	size_t GetSize() const;

	size_t GetNextId();
	D3D12_CPU_DESCRIPTOR_HANDLE GetNextCpuHandle();

	void Clear();
};

