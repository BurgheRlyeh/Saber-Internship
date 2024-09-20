#pragma once

#include "Headers.h"

#include <optional>

class DescHeapRange {
	const std::wstring& m_name{};

	size_t m_size{};
	size_t m_capacity{};

	UINT m_handleIncSize{};
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle{};
	std::optional<D3D12_DESCRIPTOR_RANGE_TYPE> m_type{};

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
	) : m_name(name)
		, m_capacity(capacity)
		, m_handleIncSize(handleIncSize)
		, m_cpuHandle(cpuHandle)
		, m_gpuHandle(gpuHandle)
		, m_type(type)
	{}
	DescHeapRange(
		const std::wstring& name,
		const DescHeapRange& other
	) : DescHeapRange(other) {}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(size_t id = 0) const {
		assert(id < m_size);
		return CD3DX12_CPU_DESCRIPTOR_HANDLE(
			m_cpuHandle,
			static_cast<UINT>(id),
			m_handleIncSize
		);
	}
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(size_t id = 0) const {
		assert(id < m_size);
		return CD3DX12_GPU_DESCRIPTOR_HANDLE(
			m_gpuHandle,
			static_cast<UINT>(id),
			m_handleIncSize
		);
	}

	size_t GetSize() const {
		return m_size;
	}

	size_t GetNextId() {
		assert(m_size < m_capacity);
		return m_size++;
	}
	D3D12_CPU_DESCRIPTOR_HANDLE GetNextCpuHandle() {
		return GetCpuHandle(GetNextId());
	}

	void Clear() {
		m_size = 0;
	}
};

