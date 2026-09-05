#include "DepthBuffer.h"

#include "CommandList.h"
#include "CommandQueue.h"
#include "Device.h"
#include "DeviceContext.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "SinglePassDownsampler.h"
#include "TextureResource.h"

namespace {
	constexpr DXGI_FORMAT FormatToDepthSrv(DXGI_FORMAT depthFormat) {
		switch (depthFormat) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		case DXGI_FORMAT_D32_FLOAT:				return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_D16_UNORM:				return DXGI_FORMAT_R16_UNORM;

		default:								return DXGI_FORMAT_UNKNOWN;
		}
	}

	constexpr DXGI_FORMAT FormatToStencilSrv(DXGI_FORMAT depthFormat) noexcept {
		switch (depthFormat) {
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:	return DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
		case DXGI_FORMAT_D24_UNORM_S8_UINT:		return DXGI_FORMAT_X24_TYPELESS_G8_UINT;

		default:								return DXGI_FORMAT_UNKNOWN;
		}
	}

	constexpr bool IsStencilFormat(DXGI_FORMAT depthFormat) noexcept {
		return FormatToStencilSrv(depthFormat) != DXGI_FORMAT_UNKNOWN;
	}
}

DepthBuffer::DepthBuffer(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	UINT64 width,
	UINT height,
	DXGI_FORMAT format,
	D3D12_RESOURCE_FLAGS flags
) : m_name(name)
{
	assert(FormatToDepthSrv(format) != DXGI_FORMAT_UNKNOWN);

	auto desc{ CD3DX12_RESOURCE_DESC::Tex2D(
		format, width, height, 1, 1, 1, 0, flags
	) };

	assert(IsDsvDesc(desc));
	m_pDsvsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Dsv, 1);

	if (IsSrvDesc(desc)) {
		m_pSrvsRange = pDeviceContext->AllocateDescRange(
			m_name,
			DescRangeType::Srv,
			1 + IsStencilFormat(format)
		);
	}

	DepthBuffer::Recreate(pDeviceContext->GetDevice(), desc);
}

void DepthBuffer::Resize(
	std::shared_ptr<Device> pDevice,
	UINT64 width,
	UINT height
) {
	auto desc{ m_pDepthBuffer->GetResourceDesc() };
	desc.Width = width;
	desc.Height = height;
	Recreate(pDevice, desc);
}

void DepthBuffer::Clear(std::shared_ptr<CommandList> pCommandList) {
	m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_pDepthBuffer->ClearDepthTarget(pCommandList, GetDsvCpuDescHandle());
}

std::shared_ptr<TextureResource> DepthBuffer::GetTexture() const {
	return m_pDepthBuffer;
}

D3D12_RESOURCE_DESC DepthBuffer::GetDesc() const {
	return m_pDepthBuffer->GetResourceDesc();
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBuffer::GetDsvCpuDescHandle() const {
	return m_pDsvsRange->GetCpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetSrvGpuDescHandle(
	DepthStencilSubresources subresource
) const {
	assert(m_pSrvsRange);
	return m_pSrvsRange->GetGpuHandle(static_cast<size_t>(subresource));
}

void DepthBuffer::Recreate(
	std::shared_ptr<Device> pDevice,
	const D3D12_RESOURCE_DESC& desc
) {
	m_pDsvsRange->FreeAll();

	m_pDepthBuffer = std::make_shared<TextureResource>(
		m_name + L"/DepthBuffer",
		pDevice,
		GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceDesc{
			desc,
			D3D12_RESOURCE_STATE_COMMON,
			&CD3DX12_CLEAR_VALUE(desc.Format, 0.0f, 0)
		}
	);

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
		.Format{ desc.Format },
		.ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
		.Flags{ D3D12_DSV_FLAG_NONE },
		.Texture2D{}
	};
	m_pDepthBuffer->CreateDepthStencilView(
		pDevice,
		m_pDsvsRange->AllocateGetCpuHandle(),
		&dsvDesc
	);

	if (!m_pSrvsRange) {
		return;
	}

	m_pSrvsRange->FreeAll();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ FormatToDepthSrv(desc.Format) },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ 1 } }
	};
	m_pDepthBuffer->CreateShaderResourceView(
		pDevice, m_pSrvsRange->AllocateGetCpuHandle(), &srvDesc
	);

	if (IsStencilFormat(desc.Format)) {
		D3D12_SHADER_RESOURCE_VIEW_DESC stencilSrvDesc{
			.Format{ FormatToStencilSrv(desc.Format) },
			.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
			.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
			.Texture2D{ .MipLevels{ 1 } }
		};

		m_pDepthBuffer->CreateShaderResourceView(
			pDevice, m_pSrvsRange->AllocateGetCpuHandle(), &stencilSrvDesc
		);
	}
}

// HiDepthBuffer

HiDepthBuffer::HiDepthBuffer(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	UINT64 width,
	UINT height,
	DXGI_FORMAT format,
	D3D12_RESOURCE_FLAGS flags
) : DepthBuffer(name, pDeviceContext, width, height, format, flags) {
	assert(IsSrvDesc(GetDesc()));	// depth read as srv during HZB pass

	m_pHzbSrvsRange = pDeviceContext->AllocateDescRange(m_name + L"/HZB", DescRangeType::Srv, 1);
	m_pHzbUavsRange = pDeviceContext->AllocateDescRange(m_name + L"/HZB", DescRangeType::Uav, HzbMaxMipCount);

	m_pSinglePassDownsampler = std::make_shared<SinglePassDownsampler>(
		pDeviceContext,
		width,
		height
	);

	RecreateHiDepthBuffer(pDeviceContext->GetDevice(), GetDesc());

	m_pDepthBufferFence = std::make_shared<EnumFence<DepthBufferState>>(
		m_name + L"/Fence",
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
	assert(IsSrvDesc(desc));	// depth read as srv during HZB pass

	m_pHzbSrvsRange->FreeAll();
	m_pHzbUavsRange->FreeAll();

	// TODO: splitting when resolution > 4096
	const size_t resMax{ std::max<size_t>(desc.Width, desc.Height) };
	if (resMax > HzbMaxResolution) {
		throw std::runtime_error("HiDepthBuffer does not support resolution larger than 4096");
	}

	const DXGI_FORMAT hzbFormat{ FormatToDepthSrv(desc.Format) };
	const UINT mipLevels{ 1u + static_cast<UINT>(std::log2f(static_cast<float>(resMax))) };

	D3D12_RESOURCE_DESC hzbDesc{ desc };
	hzbDesc.Format = hzbFormat;
	hzbDesc.MipLevels = static_cast<UINT16>(mipLevels);
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

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ hzbFormat },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ mipLevels } }
	};
	m_pHZBuffer->CreateShaderResourceView(
		pDevice,
		m_pHzbSrvsRange->AllocateGetCpuHandle(),
		&srvDesc
	);

	for (size_t i{}; i < mipLevels; ++i) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			.Format{ hzbFormat },
			.ViewDimension{ D3D12_UAV_DIMENSION_TEXTURE2D },
			.Texture2D{ .MipSlice{ static_cast<UINT>(i) } }
		};
		m_pHZBuffer->CreateUnorderedAccessView(pDevice, m_pHzbUavsRange->AllocateGetCpuHandle(), &uavDesc);
	}

	m_pSinglePassDownsampler->Resize(pDevice, desc.Width, desc.Height);
}

void HiDepthBuffer::CreateHierarchicalDepthBuffer(
	std::shared_ptr<CommandList> pCommandList,
	const std::shared_ptr<DescriptorHeap>& pResDescHeap
) {
	// copy original depth-buffer as mip 0
	// TODO: make copy part of the spd shader
	size_t width{ m_pDepthBuffer->GetWidth() };
	size_t height{ m_pDepthBuffer->GetHeight() };
	m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	pCommandList->GetD3D12CommandList()->CopyTextureRegion(
		&CD3DX12_TEXTURE_COPY_LOCATION(m_pHZBuffer->GetD3D12Resource().Get(), 0),
		0, 0, 0,
		&CD3DX12_TEXTURE_COPY_LOCATION(m_pDepthBuffer->GetD3D12Resource().Get(), 0),
		&CD3DX12_BOX(0, 0, 0, static_cast<LONG>(width), static_cast<LONG>(height), 1)
	);

	// run single pass downsampler
	m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_pSinglePassDownsampler->Dispatch(
		pCommandList,
		pResDescHeap->GetD3D12DescriptorHeap(),
		GetSrvGpuDescHandle(),
		GetUavGpuDescHandleForMidMip(),
		GetUavGpuDescHandleForMips()
	);
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetSrvGpuDescHandleWithMips() const {
	return m_pHzbSrvsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetUavGpuDescHandle() const {
	return m_pHzbUavsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetUavGpuDescHandleForMips() const {
	return m_pHzbUavsRange->GetGpuHandle(1);
}

D3D12_GPU_DESCRIPTOR_HANDLE HiDepthBuffer::GetUavGpuDescHandleForMidMip() const {
	return m_pHzbUavsRange->GetGpuHandle(HzbMidMipUavId);
}

std::shared_ptr<EnumFence<DepthBufferState>> HiDepthBuffer::GetFence() const {
	return m_pDepthBufferFence;
}

void HiDepthBuffer::SignalState(std::shared_ptr<CommandList>& pCommandList, DepthBufferState state) {
	pCommandList->AddAfterTask([&, state] {
		pCommandList->GetQueue()->Signal(m_pDepthBufferFence, state);
	});
}

void HiDepthBuffer::WaitState(std::shared_ptr<CommandList>& pCommandList, DepthBufferState state) {
	pCommandList->AddBeforeTask([state, pQueue = pCommandList->GetQueue(), pFence = m_pDepthBufferFence] {
		pQueue->GpuWait(pFence, state);
	});

	switch (state) {
	case DepthBufferState::InvalidState:
		assert(false);
		break;

	case DepthBufferState::DepthWriting:
		m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		break;

	case DepthBufferState::HierarchicalDepthBuilding:
		m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
		m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
		break;

	case DepthBufferState::DepthReading:
		m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		break;

	case DepthBufferState::FlushState:
		m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COMMON);
		m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COMMON);
		break;
	}
}
