#include "GPUResource.h"

#include <stdexcept>
#include <cassert>

#include "CommandList.h"
#include "Device.h"
#include "DeviceContext.h"

std::shared_ptr<GPUResource> GPUResource::pCounterResetter = nullptr;

GPUResource::GPUResource(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const AllocationDesc& allocDesc,
	const ResourceDesc& resDesc
) {
	CreateResource(name, pDevice, allocDesc, resDesc);
}

void GPUResource::ResourceTransition(
	std::shared_ptr<CommandList> pCommandList,
	const D3D12_RESOURCE_STATES& toState
) {
	pCommandList->GetD3D12CommandList()->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			GetD3D12Resource().Get(),
			m_state,
			toState
		)
	);
	m_state = toState;
}

Microsoft::WRL::ComPtr<ID3D12Resource> GPUResource::GetD3D12Resource() const {
	return m_pResource;
}

std::shared_ptr<GPUResource> GPUResource::CreateIntermediate(
	std::shared_ptr<Device> pDevice,
	UINT firstSubresource,
	UINT numSubresources
) {
	D3D12_RESOURCE_DESC resDesc{ CD3DX12_RESOURCE_DESC::Buffer(
		GetRequiredIntermediateSize(GetD3D12Resource().Get(), firstSubresource, numSubresources)
	) };
	if (resDesc.Width == static_cast<UINT64>(-1)) {
		resDesc = GetD3D12Resource()->GetDesc();
		resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	}

	return std::make_shared<GPUResource>(
		L"Intermediate",
		pDevice,
		AllocationDesc{ D3D12_HEAP_TYPE_UPLOAD },
		ResourceDesc{
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
		GetD3D12Resource().Get(),
		pIntermediate->GetD3D12Resource().Get(),
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
		GetD3D12Resource().Get(),
		pIntermediate->GetD3D12Resource().Get(),
		intermediateOffset,
		firstSubresource,
		numSubresources,
		pResourceData,
		pSrcData
	);
}

bool GPUResource::SupportsView(ResourceView viewType) const {
	switch (viewType) {
	case ResourceView::Srv: return IsSrv();
	case ResourceView::Uav: return IsUav();
	case ResourceView::Cbv: return IsCbv();
	case ResourceView::Rtv: return IsRtv();
	case ResourceView::Dsv: return IsDsv();

	default: return false;
	}
}
void GPUResource::CreateResourceView(
	ResourceView viewType,
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle
) {
	switch (viewType) {
	case ResourceView::Srv:
		CreateShaderResourceView(pDevice, cpuDescHandle);
		break;
	case ResourceView::Uav:
		CreateUnorderedAccessView(pDevice, cpuDescHandle);
		break;
	case ResourceView::Cbv:
		CreateConstantBufferView(pDevice, cpuDescHandle);
		break;
	case ResourceView::Rtv:
		CreateRenderTargetView(pDevice, cpuDescHandle);
		break;
	case ResourceView::Dsv:
		CreateDepthStencilView(pDevice, cpuDescHandle);
		break;
	default:
		throw std::runtime_error(
			"Attempt to create resource view of unknown type: "
			+ static_cast<std::underlying_type_t<ResourceView>>(viewType)
		);
	}
}

bool GPUResource::IsSrv() const {
	return IsSrvDesc(GetResourceDesc());
}
std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> GPUResource::GetSrvDesc() const {
	return std::nullopt;
}
void GPUResource::CreateShaderResourceView(
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc
) {
	assert(IsSrv());
	auto desc{ GetSrvDesc() };
	pDevice->GetD3D12Device()->CreateShaderResourceView(
		GetD3D12Resource().Get(),
		pSrvDesc ? pSrvDesc : (desc ? &*desc : nullptr),
		cpuDescHandle
	);
}

bool GPUResource::IsUav() const {
	return IsUavDesc(GetResourceDesc());
}
std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> GPUResource::GetUavDesc() const {
	return std::nullopt;
}
void GPUResource::CreateUnorderedAccessView(
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc,
	Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource
) {
	assert(IsUav());
	auto desc{ GetUavDesc() };
	pDevice->GetD3D12Device()->CreateUnorderedAccessView(
		GetD3D12Resource().Get(),
		pCounterResource.Get(),
		pUavDesc ? pUavDesc : (desc ? &*desc : nullptr),
		cpuDescHandle
	);
}

bool GPUResource::IsCbv() const {
	return IsCbvDesc(GetResourceDesc());
}
std::optional<D3D12_CONSTANT_BUFFER_VIEW_DESC> GPUResource::GetCbvDesc() const {
	return D3D12_CONSTANT_BUFFER_VIEW_DESC{
		.BufferLocation{ GetD3D12Resource()->GetGPUVirtualAddress() },
		.SizeInBytes{ AlignSize(
			GetD3D12Resource()->GetDesc().Width,
			D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
		) }
	};
}
void GPUResource::CreateConstantBufferView(
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc
) {
	assert(IsCbv());
	auto desc{ GetCbvDesc() };
	pDevice->GetD3D12Device()->CreateConstantBufferView(
		pCbvDesc ? pCbvDesc : (desc ? &*desc : nullptr),
		cpuDescHandle
	);
}

bool GPUResource::IsRtv() const {
	return IsRtvDesc(GetResourceDesc());
}
std::optional<D3D12_RENDER_TARGET_VIEW_DESC> GPUResource::GetRtvDesc() const {
	return std::nullopt;
}
void GPUResource::CreateRenderTargetView(
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc
) {
	assert(IsRtv());
	auto desc{ GetRtvDesc() };
	pDevice->GetD3D12Device()->CreateRenderTargetView(
		GetD3D12Resource().Get(),
		pRtvDesc ? pRtvDesc : (desc ? &*desc : nullptr),
		cpuDescHandle
	);
}

bool GPUResource::IsDsv() const {
	return GetD3D12Resource()->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}
std::optional<D3D12_DEPTH_STENCIL_VIEW_DESC> GPUResource::GetDsvDesc() const {
	return std::nullopt;
}
void GPUResource::CreateDepthStencilView(
	std::shared_ptr<Device> pDevice,
	const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
	const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc
) {
	assert(IsDsv());
	auto desc{ GetDsvDesc() };
	pDevice->GetD3D12Device()->CreateDepthStencilView(
		GetD3D12Resource().Get(),
		pDsvDesc ? pDsvDesc : (desc ? &*desc : nullptr),
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

void GPUResource::ClearDepthTarget(
	std::shared_ptr<CommandList> pCommandList,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
	float depth,
	const D3D12_CLEAR_FLAGS& clearFlags,
	uint8_t stencil
) {
	assert(IsDsv());
	pCommandList->GetD3D12CommandList()->ClearDepthStencilView(
		cpuDescHandle,
		clearFlags,
		depth,
		stencil,
		0,
		nullptr
	);
}

void GPUResource::CreateResource(
	const std::wstring& name,
	std::shared_ptr<Device> pDevice,
	const AllocationDesc& allocDesc,
	const ResourceDesc& resDesc
) {
	D3D12MA::ALLOCATION_DESC allocationDesc{
		.Flags{ allocDesc.allocationFlags },
		.HeapType{ allocDesc.heapType },
		.ExtraHeapFlags{ allocDesc.heapFlags }
	};

	ThrowIfFailed(pDevice->GetD3D12Allocator()->CreateResource(
		&allocationDesc,
		&resDesc.resDesc,
		resDesc.resInitState,
		resDesc.pResClearValue,
		&m_pAllocation,
		IID_PPV_ARGS(&m_pResource)
	));
	m_pResource->SetName(name.c_str());
	m_state = resDesc.resInitState;
}

// CounterResetter related methods
void GPUResource::InitCounterResetter(
	std::shared_ptr<DeviceContext> pDeviceContext,
	std::shared_ptr<CommandList> pCommandList
) {
	if (pCounterResetter) {
		return;
	}

	pCounterResetter = std::make_shared<GPUResource>(
		L"GPUResource/CounterResetter",
		pDeviceContext->GetDevice(),
		AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
		ResourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)) }
	);

	UINT zero{};
	D3D12_SUBRESOURCE_DATA subresData{
		.pData{ &zero },
		.RowPitch{ sizeof(UINT) },
		.SlicePitch{ subresData.RowPitch }
	};

	std::shared_ptr<GPUResource> pIntermediate{
		pCounterResetter->CreateIntermediate(pDeviceContext->GetDevice())
	};
	pCounterResetter->UpdateSubresources(
		pCommandList,
		pIntermediate,
		&subresData
	);
	pDeviceContext->AddIntermediate(pIntermediate);

	pCounterResetter->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
}

void GPUResource::DestroyCounterResetter() {
	pCounterResetter.reset();
}

void GPUResource::ResetCounter(
	std::shared_ptr<CommandList> pCommandList,
	uint64_t counterOffset
) const {
	assert(pCounterResetter);

	pCommandList->GetD3D12CommandList()->CopyBufferRegion(
		GetD3D12Resource().Get(),
		counterOffset,
		pCounterResetter->GetD3D12Resource().Get(),
		0,
		sizeof(UINT)
	);
}

// Helper functions
UINT AlignSize(UINT size, UINT alignment) {
	return (size + (alignment - 1)) & ~(alignment - 1);
}

bool SupportsView(ResourceView viewType, const D3D12_RESOURCE_DESC& desc) {
	switch (viewType) {
	case ResourceView::Srv: return IsSrvDesc(desc);
	case ResourceView::Uav: return IsUavDesc(desc);
	case ResourceView::Cbv: return IsCbvDesc(desc);
	case ResourceView::Rtv: return IsRtvDesc(desc);
	case ResourceView::Dsv: return IsDsvDesc(desc);

	default: return false;
	}
}
bool IsCbvDesc(const D3D12_RESOURCE_DESC& desc) {
	return desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER;
}
bool IsSrvDesc(const D3D12_RESOURCE_DESC& desc) {
	return !(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
}
bool IsUavDesc(const D3D12_RESOURCE_DESC& desc) {
	return desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
}
bool IsRtvDesc(const D3D12_RESOURCE_DESC& desc) {
	return desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
}
bool IsDsvDesc(const D3D12_RESOURCE_DESC& desc) {
	return desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
}
