#include "DescriptorHeapRange.h"

DescHeapRange::DescHeapRange(
	const std::wstring& name,
	const size_t& capacity,
	const UINT& handleIncSize,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuHandle,
	const D3D12_GPU_DESCRIPTOR_HANDLE& gpuHandle,
	const std::optional<D3D12_DESCRIPTOR_RANGE_TYPE>& type
) : m_name(name)
	, m_capacity(capacity)
	, m_handleIncSize(handleIncSize)
	, m_cpuHandle(cpuHandle)
	, m_gpuHandle(gpuHandle)
	, m_type(type)
{}

DescHeapRange::DescHeapRange(
	const std::wstring & name,
	const DescHeapRange & other
) : DescHeapRange(other) {}

D3D12_CPU_DESCRIPTOR_HANDLE DescHeapRange::GetCpuHandle(size_t id) const {
	assert(id < m_size);
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_cpuHandle,
		static_cast<UINT>(id),
		m_handleIncSize
	);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescHeapRange::GetGpuHandle(size_t id) const {
	assert(id < m_size);
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_gpuHandle,
		static_cast<UINT>(id),
		m_handleIncSize
	);
}

size_t DescHeapRange::GetSize() const {
	return m_size;
}

size_t DescHeapRange::GetNextId() {
	assert(m_size < m_capacity);
	return m_size++;
}

D3D12_CPU_DESCRIPTOR_HANDLE DescHeapRange::GetNextCpuHandle() {
	return GetCpuHandle(GetNextId());
}

void DescHeapRange::Clear() {
	m_size = 0;
}
