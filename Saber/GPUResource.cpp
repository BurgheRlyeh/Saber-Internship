#include "GPUResource.h"

#include "CommandList.h"
#include "CommandQueue.h"
#include "Device.h"

std::shared_ptr<GPUResource> GPUResource::pCounterResetter = nullptr;

GPUResource::GPUResource(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	CreateResource(name, pDevice, heapData, resData, allocationFlags);
}

void GPUResource::ResourceTransition(
	std::shared_ptr<CommandList> pCommandList,
	const D3D12_RESOURCE_STATES& toState
) {
	pCommandList->GetD3D12CommandList()->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			GetResource().Get(),
			m_state,
			toState
		)
	);
	m_state = toState;
}

void GPUResource::ResetCounter(
	std::shared_ptr<CommandList> pCommandList,
	uint64_t counterOffset
) const {
	assert(pCounterResetter);

	pCommandList->GetD3D12CommandList()->CopyBufferRegion(
		GetResource().Get(),
		counterOffset,
		pCounterResetter->GetResource().Get(),
		0,
		sizeof(UINT)
	);
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetResource() const {
	return m_pResource;
}

std::shared_ptr<GPUResource> GPUResource::CreateIntermediate(
	std::shared_ptr<Device> pDevice,
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
		pDevice,
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
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc
) {
	assert(IsSrv());
	pDevice->GetD3D12Device()->CreateShaderResourceView(
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
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource
) {
	assert(IsUav());
	pDevice->GetD3D12Device()->CreateUnorderedAccessView(
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
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc
) {
	assert(IsRtv());
	pDevice->GetD3D12Device()->CreateRenderTargetView(
		GetResource().Get(),
		pRtvDesc ? pRtvDesc : GetRtvDesc(),
		cpuDescHandle
	);
}

void GPUResource::ClearRenderTarget(
	std::shared_ptr<CommandList> pCommandList,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
	const float* clearColor
) {
	assert(IsRtv());

	static float defaultColor[]{ 0.f, 0.f, 0.f, 1.f };
	pCommandList->GetD3D12CommandList()->ClearRenderTargetView(
		cpuDescHandle,
		clearColor ? clearColor : defaultColor,
		0,
		nullptr
	);
}

void GPUResource::CreateResource(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const HeapData& heapData,
	const ResourceData& resData,
	const D3D12MA::ALLOCATION_FLAGS& allocationFlags
) {
	D3D12MA::ALLOCATION_DESC allocationDesc{
		.Flags{ allocationFlags },
		.HeapType{ heapData.heapType },
		.ExtraHeapFlags{ heapData.heapFlags }
	};

	ThrowIfFailed(pDevice->GetD3D12Allocator()->CreateResource(
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
