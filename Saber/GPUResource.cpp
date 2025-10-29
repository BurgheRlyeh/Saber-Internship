#include "GPUResource.h"

std::shared_ptr<GPUResource> GPUResource::m_pCounterResetter = nullptr;

GPUResource::GPUResource(
	const std::wstring& name,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	CreateResource(name, pAllocator, heapData, resData, allocationFlags);
}

void GPUResource::ResetCounter(
	std::shared_ptr<CommandList> pCommandList,
	uint64_t counterOffset
) const {
	assert(m_pCounterResetter);

	pCommandList->GetD3D12CommandList()->CopyBufferRegion(
		GetResource().Get(),
		counterOffset,
		m_pCounterResetter->GetResource().Get(),
		0,
		sizeof(UINT)
	);
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetResource() const {
	return m_pResource;
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
		L"Intermediate",
		pAllocator,
		HeapData{ D3D12_HEAP_TYPE_UPLOAD },
		ResourceData{
			resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ
		}
	);
}

void GPUResource::UpdateSubresources(
	std::shared_ptr<CommandList> pCommandList,
	std::shared_ptr<GPUResource>& pIntermediate,
	const D3D12_SUBRESOURCE_DATA* pSrcData,
	UINT64 intermediateOffset,
	UINT firstSubresource,
	UINT numSubresources
) {
	::UpdateSubresources(
		pCommandList->GetD3D12CommandList().Get(),
		GetResource().Get(),
		pIntermediate->GetResource().Get(),
		intermediateOffset,
		firstSubresource,
		numSubresources,
		pSrcData
	);
}

void GPUResource::UpdateSubresources(
	std::shared_ptr<CommandList> pCommandList,
	std::shared_ptr<GPUResource>& pIntermediate,
	void* pResourceData,
	const D3D12_SUBRESOURCE_INFO* pSrcData,
	UINT64 intermediateOffset,
	UINT firstSubresource,
	UINT numSubresources
) {
	::UpdateSubresources(
		pCommandList->GetD3D12CommandList().Get(),
		GetResource().Get(),
		pIntermediate->GetResource().Get(),
		intermediateOffset,
		firstSubresource,
		numSubresources,
		pResourceData,
		pSrcData
	);
}


bool GPUResource::IsSrv() const {
	return IsSrvDesc(GetResource()->GetDesc());
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
	return IsUavDesc(GetResource()->GetDesc());
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
	return IsRtvDesc(GetResource()->GetDesc());
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
	const std::wstring& name,
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
		IID_PPV_ARGS(&m_pResource)
	));
	m_pResource->SetName(name.c_str());
	m_state = resData.resInitState;
}
