#include "DepthBuffer.h"

#include "CommandList.h"
#include "Device.h"
#include "DeviceContext.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "SinglePassDownsampler.h"
#include "TextureResource.h"

// helper functions
namespace {
constexpr DXGI_FORMAT FormatToDepthSrv(DXGI_FORMAT depthFmt) noexcept {
	switch (depthFmt) {
	case DXGI_FORMAT_D32_FLOAT:				return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_D16_UNORM:				return DXGI_FORMAT_R16_UNORM;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

	default:								return DXGI_FORMAT_UNKNOWN;
	}
}

constexpr DXGI_FORMAT FormatToStencilSrv(DXGI_FORMAT depthFmt) noexcept {
	switch (depthFmt) {
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

	default:								return DXGI_FORMAT_UNKNOWN;
	}
}
}

DepthBuffer::DepthBuffer(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	UINT64 width,
	UINT height,
	DXGI_FORMAT format,
	DepthBufferType type,
	D3D12_RESOURCE_FLAGS flags,
	size_t extraSrvCount
) : m_name(name), m_type(type) {
	assert(type == DepthBufferType::DepthOnly
		|| format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT || format == DXGI_FORMAT_D24_UNORM_S8_UINT);

	m_pDsvsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Dsv, 1);
	m_pSrvsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Srv,
		flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE ? 0 : 1 + static_cast<size_t>(type) + extraSrvCount);

	Recreate(
		pDeviceContext->GetDevice(),
		CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 0, 1, 0, flags)
	);
}

DepthBuffer::DepthBuffer(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	UINT64 width,
	UINT height,
	DXGI_FORMAT format,
	DepthBufferType type,
	D3D12_RESOURCE_FLAGS flags
) : DepthBuffer(name, pDeviceContext, width, height, format, type, flags, 0) {}

void DepthBuffer::Resize(
	std::shared_ptr<Device> pDevice,
	UINT64 width,
	UINT height
) {
	auto desc{ m_pDepthTarget->GetResourceDesc() };
	desc.Width = width;
	desc.Height = height;
	Recreate(pDevice, desc);
}

void DepthBuffer::Clear(std::shared_ptr<CommandList> pCommandList) {
	m_pDepthTarget->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_pDepthTarget->ClearDepthTarget(pCommandList, GetDsvCpuDescHandle());
}

std::shared_ptr<TextureResource> DepthBuffer::GetTexture() const {
	return m_pDepthTarget;
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBuffer::GetDsvCpuDescHandle() const {
	return m_pDsvsRange->GetCpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetSrvGpuDescHandle(DepthStencilSubresources subresource) const {
	return m_pSrvsRange->GetGpuHandle(static_cast<size_t>(subresource));
}

void DepthBuffer::Recreate(
	std::shared_ptr<Device> pDevice,
	const D3D12_RESOURCE_DESC& desc
) {
	m_pDsvsRange->FreeAll();
	m_pSrvsRange->FreeAll();

	m_pDepthTarget = std::make_shared<TextureResource>(
		m_name + L"/DepthBuffer",
		pDevice,
		GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceDesc{
			desc,
			D3D12_RESOURCE_STATE_COMMON,
			&CD3DX12_CLEAR_VALUE(desc.Format, 0.0f, 0)
		}
	);

	// Update views
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{
		.Format{ desc.Format },
		.ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
		.Flags{ D3D12_DSV_FLAG_NONE },
		.Texture2D{}
	};
	m_pDepthTarget->CreateDepthStencilView(
		pDevice,
		m_pDsvsRange->AllocateGetCpuHandle(),
		&dsv
	);

	if (desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE) {
		return;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC depthSrvDesc{
		.Format{ FormatToDepthSrv(desc.Format) },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ 1 } }
	};
	m_pDepthTarget->CreateShaderResourceView(
		pDevice, m_pSrvsRange->AllocateGetCpuHandle(), &depthSrvDesc
	);

	if (m_type == DepthBufferType::DepthStencil) {
		D3D12_SHADER_RESOURCE_VIEW_DESC stencilSrvDesc{
			.Format{ FormatToStencilSrv(desc.Format) },
			.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Texture2D{ .MipLevels{ 1 } }
		};

		m_pDepthTarget->CreateShaderResourceView(
			pDevice, m_pSrvsRange->AllocateGetCpuHandle(), &stencilSrvDesc
		);
	}
}

// HierarchicalDepthBuffer
HiDepthBuffer::HiDepthBuffer(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	UINT64 width,
	UINT height,
	DXGI_FORMAT format,
	DepthBufferType type,
	D3D12_RESOURCE_FLAGS flags
) : DepthBuffer(name, pDeviceContext, width, height, format, type, flags, 1) {

	m_pUavsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Uav, HzbWithMips);

	m_pSinglePassDownsampler = std::make_shared<SinglePassDownsampler>(
		pDeviceContext,
		width,
		height
	);

	RecreateHiDepthBuffer(pDeviceContext->GetDevice(), m_pDepthTarget->GetResourceDesc());

	m_pDepthBufferFence = std::make_shared<EnumFence<DepthBufferState>>(
		name + L"/Fence",
		pDeviceContext->GetDevice(),
		DepthBufferState::DepthWriting
	);
}

void HiDepthBuffer::Recreate(
	std::shared_ptr<Device> pDevice,
	const D3D12_RESOURCE_DESC& desc
) {
	DepthBuffer::Recreate(pDevice, desc);
	RecreateHiDepthBuffer(pDevice, desc);
}

void HiDepthBuffer::RecreateHiDepthBuffer(
	std::shared_ptr<Device> pDevice,
	const D3D12_RESOURCE_DESC& desc
) {
	assert(!(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE));	// HZB requires shader resource views

	m_pUavsRange->FreeAll();

	// TODO: splitting when resolution > 4096
	float resMax{ std::max<float>(desc.Width, desc.Height) };
	if (resMax > 4096.0f) {
		throw std::runtime_error("HiDepthBuffer does not support resolutions larger than 4096");
	}

	UINT mipLevels{ 1u + static_cast<UINT>(std::log2f(resMax)) };
	D3D12_RESOURCE_DESC hzbDesc{ desc };
	hzbDesc.Format = FormatToDepthSrv(desc.Format);
	hzbDesc.MipLevels = mipLevels;
	hzbDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	m_pHZBuffer = std::make_shared<TextureResource>(
		m_name + L"/HiDepthBuffer",
		pDevice,
		GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceDesc{
			hzbDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		}
	);

	DXGI_FORMAT viewFmt{ FormatToDepthSrv(desc.Format) };
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ viewFmt },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ mipLevels } }
	};
	m_hzbSrvId = m_pSrvsRange->Allocate();
	m_pHZBuffer->CreateShaderResourceView(pDevice, m_pSrvsRange->GetCpuHandle(m_hzbSrvId), &srvDesc);

	for (size_t i{}; i < mipLevels; ++i) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			.Format{ viewFmt },
			.ViewDimension{ D3D12_UAV_DIMENSION_TEXTURE2D },
			.Texture2D{ .MipSlice{ static_cast<UINT>(i) } }
		};
		m_pHZBuffer->CreateUnorderedAccessView(pDevice, m_pUavsRange->AllocateGetCpuHandle(), &uavDesc);
	}

	m_pSinglePassDownsampler->Resize(pDevice, desc.Width, desc.Height);
}

void HiDepthBuffer::CreateHierarchicalDepthBuffer(
	std::shared_ptr<CommandList> pCommandList,
	std::shared_ptr<DescriptorHeap> pResDescHeapManager
) {
	// copy original depth-buffer as mip 0
	m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	m_pDepthTarget->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	pCommandList->GetD3D12CommandList()->CopyTextureRegion(
		&CD3DX12_TEXTURE_COPY_LOCATION(m_pHZBuffer->GetD3D12Resource().Get(), 0),
		0, 0, 0,
		&CD3DX12_TEXTURE_COPY_LOCATION(m_pDepthTarget->GetD3D12Resource().Get(), 0),
		&CD3DX12_BOX(
			0, 0, 0,
			UINT(m_pDepthTarget->GetWidth()), UINT(m_pDepthTarget->GetHeight()), 1
		)
	);

	// run single pass downsampler
	m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_pDepthTarget->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_pSinglePassDownsampler->Dispatch(
		pCommandList,
		pResDescHeapManager->GetD3D12DescriptorHeap(),
		GetSrvGpuDescHandle(),
		GetUavGpuDescHandleForMidMip(),
		GetUavGpuDescHandleForMips()
	);
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetSrvGpuDescHandleWithMips() const {
	return m_pSrvsRange->GetGpuHandle(m_hzbSrvId);
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetUavGpuDescHandle() const {
	return m_pUavsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetUavGpuDescHandleForMidMip() const {
	return m_pUavsRange->GetGpuHandle(HzbMidMip);
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetUavGpuDescHandleForMips() const {
	return m_pUavsRange->GetGpuHandle(1);
}

std::shared_ptr<EnumFence<DepthBufferState>> HiDepthBuffer::GetFence() const {
	return m_pDepthBufferFence;
}
