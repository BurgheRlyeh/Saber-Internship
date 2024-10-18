#include "GPUResource.h"

GPUResource::GPUResource(
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) : m_flags(resData.resDesc.Flags) {
	CreateResource(pAllocator, heapData, resData, allocationFlags);
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetResource() const {
	return m_pAllocation->GetResource();
}


bool GPUResource::IsSrv() const {
	return !(m_flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
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
	return m_flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
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
	return m_flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
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
