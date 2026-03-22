#pragma once

#include "Headers.h"

#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "TextureResource.h"

class Texture {
	std::wstring m_name{};

	D3D12_RESOURCE_DESC m_desc{};
	UINT64 m_width{};
	UINT m_height{};

	size_t m_capacity{};

	std::vector<std::shared_ptr<TextureResource>> m_pTextures{};

	std::shared_ptr<DescRange> m_pSrvsRange{};
	std::shared_ptr<DescRange> m_pRtvsRange{};
	std::shared_ptr<DescRange> m_pUavsRange{};

public:
	Texture(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		const D3D12_RESOURCE_DESC& desc,
		size_t capacity = 1
	) : m_name(name),
		m_desc(desc),
		m_width(desc.Width),
		m_height(desc.Height),
		m_capacity(capacity)
	{
		if (IsSrvDesc(desc)) {
			m_pSrvsRange = pDeviceContext->GetDescriptorHeap()->AllocateRange(m_name + L"/Ranges/SRV", m_capacity, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
		}
		if (IsUavDesc(desc)) {
			m_pUavsRange = pDeviceContext->GetDescriptorHeap()->AllocateRange(m_name + L"/Ranges/UAV", m_capacity, D3D12_DESCRIPTOR_RANGE_TYPE_UAV);
		}
		if (IsRtvDesc(desc)) {
			m_pRtvsRange = pDeviceContext->GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)->AllocateRange(m_name + L"/Ranges/RTV", m_capacity);
		}

		Resize(pDeviceContext->GetDevice(), desc.Width, desc.Height);
	}

	size_t GetSize() const {
		return m_capacity;
	}

	UINT64 GetWidth() const {
		return m_width;
	}

	UINT64 GetHeight() const {
		return m_height;
	}

	D3D12_RESOURCE_STATES GetState() const {
		return m_pTextures[0]->GetState();
	}

	void Resize(
		std::shared_ptr<Device> pDevice,
		UINT64 width,
		UINT height
	) {
		if (m_pRtvsRange) {
			m_pRtvsRange->Clear();
		}
		if (m_pSrvsRange) {
			m_pSrvsRange->Clear();
		}
		if (m_pUavsRange) {
			m_pUavsRange->Clear();
		}

		m_pTextures.clear();
		m_pTextures.resize(m_capacity);

		m_desc.Width = width;
		m_desc.Height = height;

		for (size_t i{}; i < m_capacity; ++i) {
			m_pTextures[i] = std::make_shared<TextureResource>(
				m_name + L"/Texture" + std::to_wstring(i),
				pDevice,
				GPUResource::AllocationDesc{},
				GPUResource::ResourceDesc{ m_desc }
			);
			if (m_pRtvsRange) {
				m_pTextures[i]->CreateRenderTargetView(pDevice, m_pRtvsRange->GetNextCpuHandle());
			}
			if (m_pSrvsRange) {
				m_pTextures[i]->CreateShaderResourceView(pDevice, m_pSrvsRange->GetNextCpuHandle());
			}
			if (m_pUavsRange) {
				m_pTextures[i]->CreateUnorderedAccessView(pDevice, m_pUavsRange->GetNextCpuHandle());
			}
		}
	}

	void Clear(
		std::shared_ptr<CommandList> pCommandList,
		const float* pClearValue = nullptr
	) {
		assert(m_pRtvsRange);
		for (size_t i{}; i < m_capacity; ++i) {
			m_pTextures[i]->ClearRenderTarget(pCommandList, m_pRtvsRange->GetCpuHandle(i), pClearValue);
		}
	}

	std::shared_ptr<TextureResource> GetTexture(size_t id) const {
		return m_pTextures[id];
	}

	void ChangeState(
		std::shared_ptr<CommandList> pCommandList,
		const D3D12_RESOURCE_STATES& toState
	) {
		for (auto& pTex : m_pTextures) {
			pTex->ResourceTransition(pCommandList, toState);
		}
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvDescHandle(size_t id = 0) const {
		assert(m_pSrvsRange);
		return m_pSrvsRange ? m_pSrvsRange->GetGpuHandle(id) : static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(0);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetRtvDescHandle(size_t id = 0) const {
		assert(m_pRtvsRange);
		return m_pRtvsRange ? m_pRtvsRange->GetGpuHandle(id) : static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(0);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavDescHandle(size_t id = 0) const {
		assert(m_pUavsRange);
		return m_pUavsRange ? m_pUavsRange->GetGpuHandle(id) : static_cast<D3D12_GPU_DESCRIPTOR_HANDLE>(0);
	}

	D3D12_RT_FORMAT_ARRAY GetRtFormatArray() const {
		assert(m_pRtvsRange);

		UINT numRenderTargets{ m_pRtvsRange ? static_cast<UINT>(m_pRtvsRange->GetSize()) : 0 };
		assert(numRenderTargets == m_capacity);

		D3D12_RT_FORMAT_ARRAY rtFormats{ .NumRenderTargets{ numRenderTargets } };
		for (size_t i{}; i < numRenderTargets; ++i) {
			rtFormats.RTFormats[i] = m_desc.Format;
		}
		return rtFormats;
	}
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetRtvs() const {
		assert(m_pRtvsRange);

		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs{ m_pRtvsRange->GetSize() };
		for (size_t i{}; i < m_pRtvsRange->GetSize(); ++i) {
			rtvs[i] = m_pRtvsRange->GetCpuHandle(i);
		}
		return rtvs;
	}

	void SetRenderTargets(
		std::shared_ptr<CommandList> pCommandList,
		std::shared_ptr<DepthBuffer> pDepthBuffer = nullptr
	) const {
		assert(m_pRtvsRange); 
		pCommandList->GetD3D12CommandList()->OMSetRenderTargets(
			static_cast<UINT>(m_pRtvsRange->GetSize()),
			&(m_pRtvsRange->GetCpuHandle()),
			TRUE,
			pDepthBuffer ? &pDepthBuffer->GetDsvCpuDescHandle() : nullptr
		);
	}
};

#include "EnumFence.h"

enum class GBufferState : uint8_t {
	InvalidState = 0,

	Write,
	Read,

	FlushState = std::numeric_limits<uint8_t>::max()
};

class GBuffer : public Texture {
	static inline constexpr size_t GBufferSize{ 3 };
	static constexpr D3D12_RESOURCE_DESC GetGBufferTexDesc(size_t width, size_t height) {
		return D3D12_RESOURCE_DESC{
			.Dimension{ D3D12_RESOURCE_DIMENSION_TEXTURE2D },
			.Width{ static_cast<UINT64>(width) },
			.Height{ static_cast<UINT>(height) },
			.DepthOrArraySize{ 1 },
			.MipLevels{ 0 },
			.Format{ DXGI_FORMAT_R32G32B32A32_FLOAT },
			.SampleDesc{ 1, 0 },
			.Flags{ D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET }
		};
	}

	std::shared_ptr<EnumFence<GBufferState>> m_pGBufferFence{};

public:
	GBuffer(
		const std::wstring& name,
		std::shared_ptr<DeviceContext> pDeviceContext,
		size_t width,
		size_t height
	) : Texture(name, pDeviceContext, GetGBufferTexDesc(width, height), GBufferSize),
		m_pGBufferFence(std::make_shared<EnumFence<GBufferState>>(
			name + L"/Fence",
			pDeviceContext->GetDevice(),
			GBufferState::Write
		))
	{}

	std::shared_ptr<EnumFence<GBufferState>> GetFence() const {
		return m_pGBufferFence;
	}
};
