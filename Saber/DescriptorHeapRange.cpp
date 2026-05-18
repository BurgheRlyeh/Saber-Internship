#include "DescriptorHeapRange.h"

#include "DescriptorHeapManager.h"

#include <stdexcept>

// DescRange

DescRange::DescRange(
	const std::wstring& name,
	std::shared_ptr<DescriptorHeap>& pDescHeapManager,
	size_t startId,
	size_t capacity
) : m_name(name),
	m_pDescHeapManager(pDescHeapManager),
	m_startId(startId),
	m_capacity(capacity)
{}

D3D12_CPU_DESCRIPTOR_HANDLE DescRange::GetCpuHandle(size_t id) const {
	assert(id < m_capacity);
	
	auto pDescHeapManager{ m_pDescHeapManager.lock() };
	assert(pDescHeapManager);

	return pDescHeapManager->GetCpuHandle(m_startId + id);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescRange::GetGpuHandle(size_t id) const {
	assert(id < m_capacity);

	auto pDescHeapManager{ m_pDescHeapManager.lock() };
	assert(pDescHeapManager);

	return pDescHeapManager->GetGpuHandle(m_startId + id);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescRange::AllocateGetCpuHandle() {
	return GetCpuHandle(Allocate());
}

void DescRange::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) {
	assert(GetCpuHandle(0).ptr <= cpuHandle.ptr && cpuHandle.ptr <= GetCpuHandle(m_capacity - 1).ptr);

	auto pDescHeapManager{ m_pDescHeapManager.lock() };
	assert(pDescHeapManager);

	auto handleIncSize{ pDescHeapManager->GetHandleIncrementSize() };
	auto rangeCpuHandle{ pDescHeapManager->GetCpuHandle(m_startId) };
	
	Free((cpuHandle.ptr - rangeCpuHandle.ptr) / handleIncSize);
}

// StackDescRange

size_t StackDescRange::Allocate() {
	assert(m_size < m_capacity);
	return m_size++;
}

void StackDescRange::Free(size_t id) {
	assert(id == m_size - 1);
	--m_size;
}

void StackDescRange::FreeAll() {
	m_size = 0;
}

// PoolDescRange

PoolDescRange::PoolDescRange(
	const std::wstring& name,
	std::shared_ptr<DescriptorHeap>& pDescHeapManager,
	size_t startId,
	size_t capacity
) : DescRange(name, pDescHeapManager, startId, capacity) {
	FreeAll();
}

size_t PoolDescRange::Allocate() {
	assert(!m_freeIndices.empty());

	const size_t id{ m_freeIndices.back() };
	m_freeIndices.pop_back();

	return id;
}

void PoolDescRange::Free(size_t id) {
	assert(id < m_capacity);
	m_freeIndices.push_back(id);
}

void PoolDescRange::FreeAll() {
	m_freeIndices.clear();
	m_freeIndices.reserve(m_capacity);
	for (size_t i{}; i < m_capacity; ++i) {
		m_freeIndices.push_back(m_capacity - i - 1);
	}
}
