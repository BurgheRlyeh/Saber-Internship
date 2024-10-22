#include "GPUResource.h"

GPUResource::GPUResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	CreateResource(pAllocator, heapData, resData, allocationFlags);
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetResource() const {
	return m_pAllocation->GetResource();
}

std::shared_ptr<GPUResource> GPUResource::CreateIntermediate(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	UINT firstSubresource,
	UINT numSubresources
) {
	D3D12_RESOURCE_DESC resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
		GetRequiredIntermediateSize(GetResource().Get(), firstSubresource, numSubresources)
	) };
	if (resDesc.Width == static_cast<UINT64>(-1)) {
		resDesc = GetResource()->GetDesc();
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	return std::make_shared<GPUResource>(
		pAllocator,
		HeapData{ D3D12_HEAP_TYPE_UPLOAD },
		ResourceData{
			resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ
		}
	);
}

std::shared_ptr<GPUResource> GPUResource::UpdateSubresource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandList> pCommandList,
	UINT firstSubresource,
	UINT numSubresources,
	const D3D12_SUBRESOURCE_DATA* pSrcData
) {
	std::shared_ptr<GPUResource> pIntermediate{
		CreateIntermediate(pAllocator, firstSubresource, numSubresources)
	};
	UpdateSubresources(
		pCommandList->m_pCommandList.Get(),
		GetResource().Get(),
		pIntermediate->GetResource().Get(),
		0,
		firstSubresource,
		numSubresources,
		pSrcData
	);
	return pIntermediate;
}

std::shared_ptr<GPUResource> GPUResource::UpdateSubresource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<CommandList> pCommandList,
	UINT firstSubresource,
	UINT numSubresources,
	void* pResourceData,
	const D3D12_SUBRESOURCE_INFO* pSrcData
) {
	std::shared_ptr<GPUResource> pIntermediate{
		CreateIntermediate(pAllocator, firstSubresource, numSubresources)
	};
	UpdateSubresources(
		pCommandList->m_pCommandList.Get(),
		GetResource().Get(),
		pIntermediate->GetResource().Get(),
		0,
		firstSubresource,
		numSubresources,
		pResourceData,
		pSrcData
	);
	return pIntermediate;
}


bool GPUResource::IsSrv() const {
	return !(GetResource()->GetDesc().Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
}
const D3D12_SHADER_RESOURCE_VIEW_DESC* GPUResource::GetSrvDesc() const {
	return nullptr;
}
void GPUResource::CreateShaderResourceView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc
) {
	assert(IsSrv());
	pDevice->CreateShaderResourceView(
		GetResource().Get(),
		pSrvDesc ? pSrvDesc : GetSrvDesc(),
		cpuDescHandle
	);
}

bool GPUResource::IsUav() const {
	return GetResource()->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
}
const D3D12_UNORDERED_ACCESS_VIEW_DESC* GPUResource::GetUavDesc() const {
	return nullptr;
}
void GPUResource::CreateUnorderedAccessView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource
) {
	assert(IsUav());
	pDevice->CreateUnorderedAccessView(
		GetResource().Get(),
		pCounterResource.Get(),
		pUavDesc ? pUavDesc : GetUavDesc(),
		cpuDescHandle
	);
}

bool GPUResource::IsRtv() const {
	return GetResource()->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
}
const D3D12_RENDER_TARGET_VIEW_DESC* GPUResource::GetRtvDesc() const {
	return nullptr;
}
void GPUResource::CreateRenderTargetView(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc
) {
	assert(IsRtv());
	pDevice->CreateRenderTargetView(
		GetResource().Get(),
		pRtvDesc ? pRtvDesc : GetRtvDesc(),
		cpuDescHandle
	);
}

void GPUResource::CreateResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	D3D12MA::ALLOCATION_DESC allocationDesc{
		.Flags{ allocationFlags },
		.HeapType{ heapData.heapType },
		.ExtraHeapFlags{ heapData.heapFlags }
	};

	ThrowIfFailed(pAllocator->CreateResource(
		&allocationDesc,
		&resData.resDesc,
		resData.resInitState,
		resData.pResClearValue,
		&m_pAllocation,
		IID_NULL,
		nullptr
	));
}
