#pragma once

#include "Headers.h"

#include "GPUResource.h"

class Texture : public GPUResource {
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

	virtual const D3D12_RENDER_TARGET_VIEW_DESC* GetRtvDesc() const {
		return nullptr;
	};
	void CreateRenderTargetView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc = nullptr
	);

	virtual const D3D12_SHADER_RESOURCE_VIEW_DESC* GetSrvDesc() const {
		return nullptr;
	};
	void CreateShaderResourceView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr
	);

	virtual const D3D12_UNORDERED_ACCESS_VIEW_DESC* GetUavDesc() const {
		return nullptr;
	};
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

	static float defaultColor[] = { 0.f, 0.f, 0.f, 1.f };	// .4f, .6f, .9f, 1.f
	pCommandList->ClearRenderTargetView(
		cpuDescHandle,
		clearColor ? clearColor : defaultColor,
		0,
		nullptr
	);
}
