#pragma once

#include "Headers.h"

#include <cmath>

#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "Texture.h"

class DepthBuffer {
	static inline D3D12_RESOURCE_DESC m_depthBufferDesc{
		CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	};
	static inline D3D12_CLEAR_VALUE m_clearValue{
		.Format{ DXGI_FORMAT_D32_FLOAT },
		.DepthStencil{ 0.0f, 0 }
	};

	std::shared_ptr<Texture> m_pDepthBuffer{};
	std::shared_ptr<Texture> m_pHZBuffer{};

	std::shared_ptr<DescHeapRange> m_pDsvsRange{};
	std::shared_ptr<DescHeapRange> m_pSrvsRange{};
	std::shared_ptr<DescHeapRange> m_pUavsRange{};

	const size_t m_hzbSize{ 12 };
	const size_t m_hzbMidMipId{ 5 };

public:
	DepthBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerDsv,
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

	void Clear(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList);

	std::shared_ptr<Texture> GetTexture() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMidMip() const;

	D3D12_DESCRIPTOR_RANGE1 GetSrvD3d12DescRange1(
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	) const;
};
