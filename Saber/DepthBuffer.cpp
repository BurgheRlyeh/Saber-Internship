#include "DepthBuffer.h"

DepthBuffer::DepthBuffer(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManagerDsv,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
	UINT64 width,
	UINT height
) {
	m_pDsvsRange = pDescHeapManagerDsv->AllocateRange(L"DepthBuffer/Ranges/DSV", 1);
	m_pSrvsRange = pDescHeapManagerCbvSrvUav->AllocateRange(L"DepthBuffer/Ranges/SRV", 1 + m_hzbSize, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
	m_pUavsRange = pDescHeapManagerCbvSrvUav->AllocateRange(L"DepthBuffer/Ranges/UAV", m_hzbSize, D3D12_DESCRIPTOR_RANGE_TYPE_UAV);

	Resize(pDevice, pAllocator, width, height);
}

void DepthBuffer::Resize(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	UINT64 width,
	UINT height
) {
	m_pDsvsRange->Clear();
	m_pSrvsRange->Clear();
	m_pUavsRange->Clear();

	m_depthBufferDesc.Width = width;
	m_depthBufferDesc.Height = height;

	m_depthBufferDesc.MipLevels = 1;
	m_depthBufferDesc.Format = DXGI_FORMAT_D32_FLOAT;
	m_depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	m_pDepthBuffer = std::make_shared<Texture>(
		pAllocator,
		GPUResource::HeapData{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceData{
			m_depthBufferDesc,
			D3D12_RESOURCE_STATE_DEPTH_WRITE,
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
		pDevice, m_pDsvsRange->GetNextCpuHandle(), &dsv
	);
	ThrowIfFailed(pDevice->GetDeviceRemovedReason());

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
		.Format{ DXGI_FORMAT_R32_FLOAT },
		.ViewDimension{ D3D12_SRV_DIMENSION_TEXTURE2D },
		.Shader4ComponentMapping{ D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING },
		.Texture2D{ .MipLevels{ 1 } }
	};
	m_pDepthBuffer->CreateShaderResourceView(
		pDevice, m_pSrvsRange->GetNextCpuHandle(), &srvDesc
	);
	ThrowIfFailed(pDevice->GetDeviceRemovedReason());

	// Hierarchical Z-Buffer
	UINT mipLevels{ 1u + static_cast<UINT>(
		std::log2f(std::max<float>(width, height))
	) };

	m_depthBufferDesc.MipLevels = mipLevels;
	m_depthBufferDesc.Format = DXGI_FORMAT_R32_FLOAT;
	m_depthBufferDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	m_pHZBuffer = std::make_shared<Texture>(
		pAllocator,
		GPUResource::HeapData{ D3D12_HEAP_TYPE_DEFAULT },
		GPUResource::ResourceData{
			m_depthBufferDesc,
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS
		}
	);
	srvDesc.Texture2D.MipLevels = mipLevels;
	for (size_t i{ 1 }; i < mipLevels; ++i) {
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{
			.Format{ DXGI_FORMAT_R32_FLOAT },
			.ViewDimension{ D3D12_UAV_DIMENSION_TEXTURE2D },
			.Texture2D{ .MipSlice{ static_cast<UINT>(i) } }
		};
		m_pHZBuffer->CreateUnorderedAccessView(pDevice, m_pUavsRange->GetNextCpuHandle(), &uavDesc);
		m_pHZBuffer->CreateShaderResourceView(pDevice, m_pSrvsRange->GetNextCpuHandle(), &srvDesc);
	}
}

void DepthBuffer::Clear(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList) {
	pCommandList->ClearDepthStencilView(
		GetDsvCpuDescHandle(),
		D3D12_CLEAR_FLAG_DEPTH,
		0.f,
		0,
		0,
		nullptr
	);
}

std::shared_ptr<Texture> DepthBuffer::GetTexture() const {
	return m_pDepthBuffer;
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthBuffer::GetDsvCpuDescHandle() const {
	return m_pDsvsRange->GetCpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetSrvGpuDescHandle() const {
	return m_pSrvsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetUavGpuDescHandle() const {
	return m_pUavsRange->GetGpuHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE DepthBuffer::GetUavGpuDescHandleForMidMip() const {
	return m_pUavsRange->GetGpuHandle(m_hzbMidMipId);
}

D3D12_DESCRIPTOR_RANGE1 DepthBuffer::GetSrvD3d12DescRange1(UINT baseShaderRegister, UINT registerSpace, D3D12_DESCRIPTOR_RANGE_FLAGS flags, UINT offsetInDescriptorsFromTableStart) const {
	return CD3DX12_DESCRIPTOR_RANGE1(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,
		baseShaderRegister,
		registerSpace,
		flags,
		offsetInDescriptorsFromTableStart
	);
}
