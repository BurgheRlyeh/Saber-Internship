#pragma once

#include "Headers.h"

#include <cmath>

#include "DescriptorHeapManager.h"
#include "DescriptorHeapRange.h"
#include "SinglePassDownsampler.h"
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
	size_t m_depthSrvId{};
	size_t m_hzbSrvId{};
	std::shared_ptr<DescHeapRange> m_pUavsRange{};

	std::shared_ptr<SinglePassDownsampler> m_pSinglePassDownsampler{};

	const size_t m_hzbSize{ 12 };
	const size_t m_hzbMidMipId{ 5 };

	size_t m_width{};
	size_t m_height{};

public:
	DepthBuffer(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerDsv,
		std::shared_ptr<DescriptorHeapManager> pDescHeapManagerCbvSrvUav,
		UINT64 width,
		UINT height,
		std::shared_ptr<SinglePassDownsampler> pSPD = nullptr
	);

	void Resize(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		UINT64 width,
		UINT height
	);
	bool ResizeHZB(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		UINT64 width,
		UINT height
	);

	void Clear(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList);

	void SetSinglePassDownsampler(
		std::shared_ptr<SinglePassDownsampler> pSPD,
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		UINT64 width,
		UINT height
	);

	void CreateHierarchicalDepthBuffer(
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap
	);

	std::shared_ptr<Texture> GetTexture() const;

	D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuDescHandle() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandleWithMips() const;

	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandle() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMidMip() const;
	D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMips() const;

	D3D12_DESCRIPTOR_RANGE1 GetSrvD3d12DescRange1(
		UINT baseShaderRegister,
		UINT registerSpace = 0,
		D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
		UINT offsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
	) const;
};
