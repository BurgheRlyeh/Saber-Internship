#include "DescriptorHeapManager.h"

#include "DescriptorHeapRange.h"
#include "Device.h"

DescriptorHeap::DescriptorHeap(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
) : m_heapDesc(heapDesc),
	m_allocator(heapDesc.NumDescriptors),
	m_releasedRanges(m_allocator)
{
	ThrowIfFailed(pDevice->GetD3D12Device()->CreateDescriptorHeap(&m_heapDesc, IID_PPV_ARGS(&m_pDescHeap)));
	m_pDescHeap->SetName(name.c_str());

	m_handleIncSize = pDevice->GetD3D12Device()->GetDescriptorHandleIncrementSize(m_heapDesc.Type);

	m_pRangesAtlas = std::make_shared<Atlas<DescRange>>(name);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCpuHandle(size_t id) const {
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		m_pDescHeap->GetCPUDescriptorHandleForHeapStart(),
		static_cast<UINT>(id),
		m_handleIncSize
	);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGpuHandle(size_t id) const {
	if (!(m_heapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE))
		return { static_cast<UINT64>(-1) };
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(
		m_pDescHeap->GetGPUDescriptorHandleForHeapStart(),
		static_cast<UINT>(id),
		m_handleIncSize
	);
}

void DescriptorHeap::FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
	std::scoped_lock<std::mutex> lock(m_mutex);
	m_releasedRanges.FinishFrame(fenceValue, lastCompletedFenceValue);
}

void DescriptorHeap::Release(size_t startId, size_t size) {
	std::scoped_lock<std::mutex> lock(m_mutex);

	// Shader-visible descriptors are read when the GPU executes, others are copied when recorded
	if (m_heapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
		m_releasedRanges.Add({ startId, size });
	}
	else {
		m_allocator.Free(startId, size);
	}
}

// DescriptorHeapManager

DescriptorHeapManager::DescriptorHeapManager(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const std::array<DescHeapArgs, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES>& descHeapArgs
) : m_name(name) {
	for (size_t i{}; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i) {
		m_pDescHeaps[i] = std::make_shared<DescriptorHeap>(
			m_name + L"/DescriptorHeap" + std::to_wstring(i),
			pDevice,
			D3D12_DESCRIPTOR_HEAP_DESC{
				static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i),
				static_cast<UINT>(descHeapArgs[i].size),
				descHeapArgs[i].flags,
				descHeapArgs[i].nodeMask
			}
		);
	}
}

void DescriptorHeapManager::FinishFrame(uint64_t fenceValue, uint64_t lastCompletedFenceValue) {
	for (const auto& pDescHeap : m_pDescHeaps) {
		pDescHeap->FinishFrame(fenceValue, lastCompletedFenceValue);
	}
}
