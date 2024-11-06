#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

#include "CommandQueue.h"

class GPUResource {
	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};

public:
	GPUResource() = default;
	virtual ~GPUResource() = default;

	struct HeapData {
		D3D12_HEAP_TYPE heapType{};
		D3D12_HEAP_FLAGS heapFlags{};
	};
	struct ResourceData {
		D3D12_RESOURCE_DESC resDesc{};
		D3D12_RESOURCE_STATES resInitState{};
		const D3D12_CLEAR_VALUE* pResClearValue{};
	};
	GPUResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	void CreateResource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() const;

	std::shared_ptr<GPUResource> CreateIntermediate(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		UINT firstSubresource,
		UINT numSubresources
	);

	void UpdateSubresource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandList> pCommandList,
		UINT firstSubresource,
		UINT numSubresources,
		const D3D12_SUBRESOURCE_DATA* pSrcData,
		std::shared_ptr<GPUResource>& pIntermediate
	);
	void UpdateSubresource(
		Microsoft::WRL::ComPtr<D3D12MA::Allocator> pAllocator,
		std::shared_ptr<CommandList> pCommandList,
		UINT firstSubresource,
		UINT numSubresources,
		void* pResourceData,
		const D3D12_SUBRESOURCE_INFO* pSrcData,
		std::shared_ptr<GPUResource>& pIntermediate
	);

	bool IsSrv() const;
	virtual const D3D12_SHADER_RESOURCE_VIEW_DESC* GetSrvDesc() const;;
	void CreateShaderResourceView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr
	);

	bool IsUav() const;
	virtual const D3D12_UNORDERED_ACCESS_VIEW_DESC* GetUavDesc() const;;
	void CreateUnorderedAccessView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc = nullptr,
		Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource = nullptr
	);

	bool IsRtv() const;
	virtual const D3D12_RENDER_TARGET_VIEW_DESC* GetRtvDesc() const;;
	void CreateRenderTargetView(
		Microsoft::WRL::ComPtr<ID3D12Device2> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc = nullptr
	);
};

static void ResourceTransition(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
	const D3D12_RESOURCE_STATES& stateBefore,
	const D3D12_RESOURCE_STATES& stateAfter
) {
	pCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			pResource.Get(),
			stateBefore,
			stateAfter
		)
	);
}

static void ClearRenderTarget(
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> pCommandList,
	Microsoft::WRL::ComPtr<ID3D12Resource> pResource,
	D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
	const float* clearColor = nullptr
) {
	assert(pResource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

	static float defaultColor[]{ 0.f, 0.f, 0.f, 1.f };
	pCommandList->ClearRenderTargetView(
		cpuDescHandle,
		clearColor ? clearColor : defaultColor,
		0,
		nullptr
	);
}
