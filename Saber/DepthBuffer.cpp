#include "DepthBuffer.h"

#include "CommandList.h"
#include "Device.h"
#include "DeviceContext.h"
#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "SinglePassDownsampler.h"
#include "TextureResource.h"

DepthBuffer::DepthBuffer(
	const std::wstring& name,
	std::shared_ptr<DeviceContext> pDeviceContext,
	UINT64 width,
	UINT height,
	std::shared_ptr<SinglePassDownsampler> pSPD
) : m_name(name),
	m_pDepthBufferFence(std::make_shared<EnumFence<DepthBufferState>>(
		name + L"/Fence",
		pDeviceContext->GetDevice(),
		DepthBufferState::DepthWriting
	)) {
	m_pDsvsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Dsv, 1);
	m_pSrvsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Srv, 2);
	m_pUavsRange = pDeviceContext->AllocateDescRange(m_name, DescRangeType::Uav, m_hzbSize);

	m_pSinglePassDownsampler = pSPD;
	Resize(pDeviceContext->GetDevice(), width, height);
}

void DepthBuffer::Resize(
	std::shared_ptr<Device> pDevice,
	UINT64 width,
	UINT height
) {
	m_width = width;
	m_height = height;

	m_pDsvsRange->FreeAll();
	m_pSrvsRange->FreeAll();
	m_pUavsRange->FreeAll();
	
	D3D12_RESOURCE_DESC resDesc{ m_depthBufferDesc };
	resDesc.Width = width;
	resDesc.Height = height;

	m_pDepthBuffer = std::make_shared<TextureResource>(
		m_name + L"/DepthBuffer",
		pDevice,
		GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceDesc{
			resDesc,
			D3D12_RESOURCE_STATE_COMMON,
			&m_clearValue
		}
	);

	// Update views
	D3D12_DEPTH_STENCIL_VIEW_DESC dsv{
		.Format{ DXGI_FORMAT_D32_FLOAT },
		.ViewDimension{ D3D12_DSV_DIMENSION_TEXTURE2D },
		.Flags{ D3D12_DSV_FLAG_NONE },
		.Texture2D{}
	};
	m_pDepthBuffer->CreateDepthStencilView(
		pDevice,
		m_pDsvsRange->AllocateGetCpuHandle(),
		&dsv
	);
	
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ DXGI_FORMAT_R32_FLOAT },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ 1 } }
	};
	m_depthSrvId = m_pSrvsRange->Allocate();
	m_pDepthBuffer->CreateShaderResourceView(
		pDevice, m_pSrvsRange->GetCpuHandle(m_depthSrvId), &srvDesc
	);

	ResizeHZB(pDevice, width, height);
}

bool DepthBuffer::ResizeHZB(
	std::shared_ptr<Device> pDevice,
	UINT64 width,
	UINT height
) {
	if (!m_pSinglePassDownsampler) {
		return false;
	}

	// TODO: splitting when resolution > 4096
	float resMax{ std::max<float>(width, height) };
	if (resMax > 4096) {
		return false;
	}

	UINT mipLevels{ 1u + static_cast<UINT>(std::log2f(resMax)) };
	D3D12_RESOURCE_DESC resDesc{ m_depthBufferDesc };
	resDesc.Width = width;
	resDesc.Height = height;
	resDesc.MipLevels = mipLevels;
	resDesc.Format = DXGI_FORMAT_R32_FLOAT;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	m_pHZBuffer = std::make_shared<TextureResource>(
		m_name + L"/HierarchicalDepthBuffer",
		pDevice,
		GPUResource::AllocationDesc{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceDesc{
			resDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		}
	);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ DXGI_FORMAT_R32_FLOAT },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ mipLevels } }
	};
	m_hzbSrvId = m_pSrvsRange->Allocate();
	m_pHZBuffer->CreateShaderResourceView(pDevice, m_pSrvsRange->GetCpuHandle(m_hzbSrvId), &srvDesc);

	for (size_t i{ 1 }; i < mipLevels; ++i) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			.Format{ DXGI_FORMAT_R32_FLOAT },
			.ViewDimension{ D3D12_UAV_DIMENSION_TEXTURE2D },
			.Texture2D{ .MipSlice{ static_cast<UINT>(i) } }
		};
		m_pHZBuffer->CreateUnorderedAccessView(pDevice, m_pUavsRange->AllocateGetCpuHandle(), &uavDesc);
	}

	m_pSinglePassDownsampler->Resize(pDevice, width, height);
	return true;
}

void DepthBuffer::Clear(std::shared_ptr<CommandList> pCommandList) {
	m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_DEPTH_WRITE);
	m_pDepthBuffer->ClearDepthTarget(pCommandList, GetDsvCpuDescHandle());
}

void DepthBuffer::SetSinglePassDownsampler(
	std::shared_ptr<SinglePassDownsampler> pSPD,
	std::shared_ptr<Device> pDevice,
	UINT64 width,
	UINT height
) {
	m_pSinglePassDownsampler = pSPD;
	ResizeHZB(pDevice, width, height);
}

void DepthBuffer::CreateHierarchicalDepthBuffer(
	std::shared_ptr<CommandList> pCommandList,
	Microsoft::WRL::ComPtr<D3D12DescriptorHeap> pDescHeap
) {
	if (!m_pSinglePassDownsampler) {
		return;
	}

	// copy original depth-buffer as mip 0
	m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_DEST);
	m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_COPY_SOURCE);
	pCommandList->GetD3D12CommandList()->CopyTextureRegion(
		&CD3DX12_TEXTURE_COPY_LOCATION(m_pHZBuffer->GetD3D12Resource().Get(), 0),
		0, 0, 0,
		&CD3DX12_TEXTURE_COPY_LOCATION(m_pDepthBuffer->GetD3D12Resource().Get(), 0),
		&CD3DX12_BOX(0, 0, 0, m_width, m_height, 1)
	);

	// run single pass downsampler
	m_pHZBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	m_pDepthBuffer->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	m_pSinglePassDownsampler->Dispatch(
		pCommandList,
		pDescHeap,
		GetSrvGpuDescHandle(),
		GetUavGpuDescHandleForMidMip(),
		GetUavGpuDescHandleForMips()
	);
}

std::shared_ptr<TextureResource> DepthBuffer::GetTexture() const {
	return m_pDepthBuffer;
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBuffer::GetDsvCpuDescHandle() const {
	return m_pDsvsRange->GetCpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetSrvGpuDescHandle() const {
	return m_pSrvsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetSrvGpuDescHandleWithMips() const {
	return m_pSrvsRange->GetGpuHandle(1);
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetUavGpuDescHandle() const {
	return m_pUavsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetUavGpuDescHandleForMidMip() const {
	return m_pUavsRange->GetGpuHandle(m_hzbMidMipId);
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetUavGpuDescHandleForMips() const {
	return m_pUavsRange->GetGpuHandle();
}

std::shared_ptr<EnumFence<DepthBufferState>> DepthBuffer::GetFence() const {
	return m_pDepthBufferFence;
}
