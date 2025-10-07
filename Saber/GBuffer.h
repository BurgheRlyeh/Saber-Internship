#pragma once

#include "Headers.h"

#include <vector>

#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "TextureResource.h"

class GBuffer {
	static inline D3D12_RESOURCE_DESC m_resDescs[]{
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0,    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),	// uvMaterial
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0,    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET), // tbn
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0,    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET), // ddxddy
		CD3DX12_RESOURCE_DESC::Tex2D(    DXGI_FORMAT_R8G8B8A8_UNORM, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)	// resulting uav
	};

	std::vector<std::shared_ptr<TextureResource>> m_pTextures{};

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
	);

	void Resize(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		UINT64 width,
		UINT height
	);

	void Clear(
		std::shared_ptr<CommandList> pCommandList,
		const float* pClearValue = nullptr
	);

	enum class State : size_t {
		RENDERING,
		DEFERRED_SHADING,
		POST_PROCESSING
	};
	void ChangeState(
		std::shared_ptr<CommandList> pCommandList,
		const State& state
	) {
		switch (state) {
		case State::RENDERING: {
			for (size_t i{}; i < m_pRtvsRange->GetSize(); ++i) {
				m_pTextures[i]->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_RENDER_TARGET);
			}
			break;
		}
		case State::DEFERRED_SHADING: {
			for (size_t i{}; i < m_pSrvsRange->GetSize(); ++i) {
				m_pTextures[i]->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			}
			for (size_t i{}; i < m_pUavsRange->GetSize(); ++i) {
				m_pTextures[i]->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}
			break;
		}
		case State::POST_PROCESSING: {
			for (size_t i{}; i < m_pSrvsRange->GetSize(); ++i) {
				m_pTextures[i]->ResourceTransition(pCommandList, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
			}
		}
		}
	}

	std::shared_ptr<TextureResource> GetTexture(size_t id) const;

	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetRtvs() const;
	D3D12_RT_FORMAT_ARRAY GetRtFormatArray() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvDescHandle(size_t id = 0) const;
	D3D12_DESCRIPTOR_RANGE1 GetSrvD3d12DescRange1(
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	) const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavDescHandle(size_t id = 0) const;
	D3D12_DESCRIPTOR_RANGE1 GetUavD3d12DescRange1(
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	) const;
};