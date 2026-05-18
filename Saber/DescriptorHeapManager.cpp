#include "DescriptorHeapManager.h"

#include "DescriptorHeapRange.h"
#include "Device.h"

DescriptorHeap::DescriptorHeap(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const D3D12_DESCRIPTOR_HEAP_DESC& heapDesc
) : m_heapDesc(heapDesc) {
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
