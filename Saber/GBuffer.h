#pragma once

#include "Headers.h"

#include <vector>

#include "DescriptorHeapManager.h"
#include "Texture.h"

class GBuffer {
	static inline D3D12_RESOURCE_DESC m_resDescs[4]{
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),	// position
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),    // normals
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),	// albedo
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)	// resulting ua
	};

	std::vector<std::shared_ptr<Texture>> m_pTextures{};

	std::shared_ptr<DescHeapRange> m_pSrvsRange{};
	std::shared_ptr<DescHeapRange> m_pUavsRange{};
	std::shared_ptr<DescHeapRange> m_pRtvsRange{};

public:
	static size_t GetSize() {
		return _countof(m_resDescs);
	}

	GBuffer(
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

	void Resize(
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
				pTex->CreateRenderTargetView(
					pDevice,
					m_pRtvsRange->GetNextCpuHandle(),
					pTex->GetRtvDesc()
				);
			}
			if (pTex->IsSrv()) {
				pTex->CreateShaderResourceView(
					pDevice,
					m_pSrvsRange->GetNextCpuHandle(),
					pTex->GetSrvDesc()
				);
			}
			if (pTex->IsUav()) {
				pTex->CreateUnorderedAccessView(
					pDevice,
					m_pUavsRange->GetNextCpuHandle(),
					pTex->GetUavDesc()
				);
			}

			m_pTextures[i] = pTex;
		}
	}

	void Clear(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		const float* pClearValue = nullptr)
	{
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

	std::shared_ptr<Texture> GetTexture(size_t id) {
		return m_pTextures[id];
	}

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetRtvs() const {
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> rtvs{ m_pRtvsRange->GetSize() };
		for (size_t i{}; i < m_pRtvsRange->GetSize(); ++i) {
			rtvs[i] = m_pRtvsRange->GetCpuHandle(i);
		}
		return rtvs;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvDescHandle(size_t id = 0) const {
		return m_pSrvsRange->GetGpuHandle(id);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavDescHandle(size_t id = 0) const {
		return m_pUavsRange->GetGpuHandle(id);
	}

	D3D12_DESCRIPTOR_RANGE1 GetSrvD3d12DescRange1(
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
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

	D3D12_DESCRIPTOR_RANGE1 GetUavD3d12DescRange1(
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
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
};