#include "DescriptorHeapRange.h"

#include "DescriptorHeapManager.h"

#include <stdexcept>

// DescRange

DescRange::DescRange(
	const std::wstring& name,
	const std::shared_ptr<DescriptorHeap>& pDescHeap,
	size_t startId,
	std::unique_ptr<IndexRange> pIds
) : m_name(name),
	m_pDescHeap(pDescHeap),
	m_startId(startId),
	m_pIds(std::move(pIds))
{
	assert(m_pDescHeap);
	assert(m_pIds);
}

DescRange::~DescRange() {
	m_pDescHeap->Release(m_startId, GetCapacity());
}

D3D12_CPU_DESCRIPTOR_HANDLE DescRange::GetCpuHandle(size_t id) const {
	assert(id < GetCapacity());
	return m_pDescHeap->GetCpuHandle(m_startId + id);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescRange::GetGpuHandle(size_t id) const {
	assert(id < GetCapacity());
	return m_pDescHeap->GetGpuHandle(m_startId + id);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescRange::AllocateGetCpuHandle() {
	return GetCpuHandle(Allocate());
}

void DescRange::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) {
	assert(GetCpuHandle(0).ptr <= cpuHandle.ptr && cpuHandle.ptr <= GetCpuHandle(GetCapacity() - 1).ptr);

	auto handleIncSize{ m_pDescHeap->GetHandleIncrementSize() };
	auto rangeCpuHandle{ m_pDescHeap->GetCpuHandle(m_startId) };

	Free((cpuHandle.ptr - rangeCpuHandle.ptr) / handleIncSize);
}
