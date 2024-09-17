#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Texture : protected GPUResource {
	D3D12_RESOURCE_FLAGS m_flags{};

protected:
	Texture() = default;

public:
	Texture(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	) : GPUResource(pAllocator, heapData, resData, allocationFlags),
		m_flags(resData.resDesc.Flags)
	{}
	using GPUResource::GetResource;

	void CreateRenderTargetView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc = nullptr
	);
	void CreateShaderResourceView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr
	);
	void CreateUnorderedAccessView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc = nullptr,
		Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource = nullptr
	);

	bool IsRtv() const;
	bool IsSrv() const;
	bool IsUav() const;
};

static void ClearRenderTarget(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> pBuffer,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
	const float* clearColor = nullptr
) {
	assert(pBuffer->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
	if (!clearColor) {
		static float defaultColor[] = {
			.4f,
			.6f,
			.9f,
			1.f
		};

		clearColor = defaultColor;
	}

	pCommandList->ClearRenderTargetView(
		cpuDescHandle,
		clearColor,
		0,
		nullptr
	);
}
