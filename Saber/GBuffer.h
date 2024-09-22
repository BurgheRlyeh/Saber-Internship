#pragma once

#include "Headers.h"

#include <vector>

#include "DescriptorHeapManager.h"
#include "Texture.h"

class GBuffer {
	static inline D3D12_RESOURCE_DESC m_resDescs[]{
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),	// position
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),    // normals
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),	// albedo
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),	// test
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
	);

	void Resize(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		UINT64 width,
		UINT height
	);

	void Clear(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		const float* pClearValue = nullptr
	);

	std::shared_ptr<Texture> GetTexture(size_t id) const;

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