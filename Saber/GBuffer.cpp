#include "GBuffer.h"

GBuffer::GBuffer(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManagerRtv,
	std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
	UINT64 width,
	UINT height
) {
	m_pRtvsRange = pDescHeapManagerRtv->AllocateRange(L"GBuffer/Ranges/RTV", GetSize() - 1);
	m_pSrvsRange = pDescHeapManagerCbvSrvUav->AllocateRange(L"GBuffer/Ranges/SRV", GetSize(), D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
	m_pUavsRange = pDescHeapManagerCbvSrvUav->AllocateRange(L"GBuffer/Ranges/UAV", 1, D3D12_DESCRIPTOR_RANGE_TYPE_UAV);

	Resize(pDevice, pAllocator, width, height);
}

void GBuffer::Resize(
	Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
	Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
	UINT64 width,
	UINT height
) {
	m_pRtvsRange->Clear();
	m_pSrvsRange->Clear();
	m_pUavsRange->Clear();

	m_pTextures.clear();
	m_pTextures.resize(GetSize());

	for (size_t i{}; i < GetSize(); ++i) {
		m_resDescs[i].Width = width;
		m_resDescs[i].Height = height;

		std::shared_ptr<Texture> pTex{ std::make_shared<Texture>(
			pAllocator,
			GPUResource::HeapData{ D3D12_HEAP_TYPE_DEFAULT },
			GPUResource::ResourceData{
				m_resDescs[i],
				m_resDescs[i].Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET ?
				D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_UNORDERED_ACCESS
			}
		) };

		if (pTex->IsRtv()) {
			pTex->CreateRenderTargetView(pDevice, m_pRtvsRange->GetNextCpuHandle());
		}
		if (pTex->IsSrv()) {
			pTex->CreateShaderResourceView(pDevice, m_pSrvsRange->GetNextCpuHandle() );
		}
		if (pTex->IsUav()) {
			pTex->CreateUnorderedAccessView(pDevice, m_pUavsRange->GetNextCpuHandle());
		}

		m_pTextures[i] = pTex;
	}
}

void GBuffer::Clear(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	const float* pClearValue
) {
	for (size_t i{}; i < m_pRtvsRange->GetSize(); ++i) {
		ResourceTransition(
			pCommandList,
			m_pTextures[i]->GetResource().Get(),
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		);

		ClearRenderTarget(
			pCommandList,
			m_pTextures[i]->GetResource(),
			m_pRtvsRange->GetCpuHandle(i),
			pClearValue
		);
	}
}

std::shared_ptr<Texture> GBuffer::GetTexture(size_t id) const {
	return m_pTextures[id];
}

std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GBuffer::GetRtvs() const {
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs{ m_pRtvsRange->GetSize() };
	for (size_t i{}; i < m_pRtvsRange->GetSize(); ++i) {
		rtvs[i] = m_pRtvsRange->GetCpuHandle(i);
	}
	return rtvs;
}

D3D12_RT_FORMAT_ARRAY GBuffer::GetRtFormatArray() const {
	D3D12_RT_FORMAT_ARRAY rtFormats{ .NumRenderTargets{ static_cast<UINT>(GetSize() - 1) } };
	for (size_t i{}; i < GetSize() - 1; ++i) {
		rtFormats.RTFormats[i] = m_resDescs[i].Format;
	}
	return rtFormats;
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetSrvDescHandle(size_t id) const {
	return m_pSrvsRange->GetGpuHandle(id);
}

D3D12_DESCRIPTOR_RANGE1 GBuffer::GetSrvD3d12DescRange1(
	UINT baseShaderRegister,
	UINT registerSpace,
	D3D12_DESCRIPTOR_RANGE_FLAGS flags,
	UINT offsetInDescriptorsFromTableStart
) const {
	return CD3DX12_DESCRIPTOR_RANGE1(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		GetSize(),
		baseShaderRegister,
		registerSpace,
		flags,
		offsetInDescriptorsFromTableStart
	);
}

D3D12_GPU_DESCRIPTOR_HANDLE GBuffer::GetUavDescHandle(size_t id) const {
	return m_pUavsRange->GetGpuHandle(id);
}

D3D12_DESCRIPTOR_RANGE1 GBuffer::GetUavD3d12DescRange1(
	UINT baseShaderRegister,
	UINT registerSpace,
	D3D12_DESCRIPTOR_RANGE_FLAGS flags,
	UINT offsetInDescriptorsFromTableStart
) const {
	return CD3DX12_DESCRIPTOR_RANGE1(
		D3D12_DESCRIPTOR_RANGE_TYPE_UAV,
		1,
		baseShaderRegister,
		registerSpace,
		flags,
		offsetInDescriptorsFromTableStart
	);
}
