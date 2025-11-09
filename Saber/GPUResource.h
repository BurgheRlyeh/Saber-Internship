#pragma once

#include "Headers.h"

#include "D3D12MemAlloc.h"

#include "CommandQueue.h"
#include "CommandList.h"

class Device;

class GPUResource {
	static std::shared_ptr<GPUResource> pCounterResetter;

	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource{};
	D3D12_RESOURCE_STATES m_state{};

	GPUResource() = default;

public:
	struct HeapData {
		D3D12_HEAP_TYPE heapType{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_HEAP_FLAGS heapFlags{};
	};
	struct ResourceData {
		D3D12_RESOURCE_DESC resDesc{};
		D3D12_RESOURCE_STATES resInitState{};
		const D3D12_CLEAR_VALUE* pResClearValue{};
	};
	GPUResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	D3D12_RESOURCE_STATES GetState() const {
		return m_state;
	}
	void ResourceTransition(
		std::shared_ptr<CommandList> pCommandList,
		const D3D12_RESOURCE_STATES& toState
	) {
		pCommandList->GetD3D12CommandList()->ResourceBarrier(
			1,
			&CD3DX12_RESOURCE_BARRIER::Transition(
				GetResource().Get(),
				m_state,
				toState
			)
		);
		m_state = toState;
	}

	void ResetCounter(
		std::shared_ptr<CommandList> pCommandList,
		uint64_t counterOffset
	) const;

	void CreateResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const HeapData& heapData,
		const ResourceData& resData,
		const D3D12MA::ALLOCATION_FLAGS& allocationFlags = D3D12MA::ALLOCATION_FLAG_NONE
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetResource() const;

	D3D12_RESOURCE_DESC GetResourceDesc() const {
		return GetResource()->GetDesc();
	}

	D3D12_HEAP_PROPERTIES GetHeapProperties() const {
		D3D12_HEAP_PROPERTIES heapProps;
		GetResource()->GetHeapProperties(&heapProps, nullptr);
		return heapProps;
	}

	D3D12_HEAP_FLAGS GetHeapFlags() const {
		D3D12_HEAP_FLAGS heapFlags;
		GetResource()->GetHeapProperties(nullptr, &heapFlags);
		return heapFlags;
	}

	std::shared_ptr<GPUResource> CreateIntermediate(
		std::shared_ptr<Device> pDevice,
		UINT firstSubresource = 0,
		UINT numSubresources = 1
	);

	void UpdateSubresources(
		std::shared_ptr<CommandList> pCommandList,
		std::shared_ptr<GPUResource>& pIntermediate,
		const D3D12_SUBRESOURCE_DATA* pSrcData,
		UINT64 intermediateOffset = 0,
		UINT firstSubresource = 0,
		UINT numSubresources = 1
	);
	void UpdateSubresources(
		std::shared_ptr<CommandList> pCommandList,
		std::shared_ptr<GPUResource>& pIntermediate,
		void* pResourceData,
		const D3D12_SUBRESOURCE_INFO* pSrcData,
		UINT64 intermediateOffset = 0,
		UINT firstSubresource = 0,
		UINT numSubresources = 1
	);

	bool IsSrv() const;
	virtual const D3D12_SHADER_RESOURCE_VIEW_DESC* GetSrvDesc() const;;
	void CreateShaderResourceView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr
	);

	bool IsUav() const;
	virtual const D3D12_UNORDERED_ACCESS_VIEW_DESC* GetUavDesc() const;;
	void CreateUnorderedAccessView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc = nullptr,
		Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource = nullptr
	);

	bool IsRtv() const;
	virtual const D3D12_RENDER_TARGET_VIEW_DESC* GetRtvDesc() const;;
	void CreateRenderTargetView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc = nullptr
	);

	void ClearRenderTarget(
		std::shared_ptr<CommandList> pCommandList,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
		const float* clearColor = nullptr
	) {
		assert(IsRtv());

		static float defaultColor[]{ 0.f, 0.f, 0.f, 1.f };
		pCommandList->GetD3D12CommandList()->ClearRenderTargetView(
			cpuDescHandle,
			clearColor ? clearColor : defaultColor,
			0,
			nullptr
		);
	}

	//static void InitCounterResetter(
	//	std::shared_ptr<DeviceContext> pDeviceContext,
	//	std::shared_ptr<CommandQueue> pCommandQueueDirect
	//) {
	//	if (pCounterResetter) {
	//		return;
	//	}

	//	pCounterResetter = std::make_shared<GPUResource>(
	//		L"GPUResource/CounterResetter",
	//		pDeviceContext->GetAllocator(),
	//		HeapData{ D3D12_HEAP_TYPE_DEFAULT },
	//		ResourceData{ CD3DX12_RESOURCE_DESC::Buffer(sizeof(UINT)) }
	//	);

	//	UINT zero{};
	//	D3D12_SUBRESOURCE_DATA subresData{
	//		.pData{ &zero },
	//		.RowPitch{ sizeof(UINT) },
	//		.SlicePitch{ subresData.RowPitch }
	//	};

	//	std::shared_ptr<GPUResource> pIntermediate{
	//		pCounterResetter->CreateIntermediate(pDeviceContext->GetAllocator())
	//	};

	//	std::shared_ptr<CommandList> pCommandListCopy{
	//		pCommandQueueDirect->GetCommandList(pDeviceContext)
	//	};
	//	pCounterResetter->UpdateSubresources(
	//		pCommandListCopy,
	//		pIntermediate,
	//		&subresData
	//	);
	//	pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListCopy);

	//	std::shared_ptr<CommandList> pCommandListDirect{
	//		pCommandQueueDirect->GetCommandList(pDeviceContext)
	//	};
	//	pCounterResetter->ResourceTransition(pCommandListDirect, D3D12_RESOURCE_STATE_COPY_SOURCE);
	//	pCommandQueueDirect->ExecuteCommandListImmediately(pCommandListDirect);
	//}
	static void DestroyCounterResetter() {
		pCounterResetter.reset();
	}
};

static UINT AlignSize(UINT size, UINT alignment) {
	return (size + (alignment - 1)) & ~(alignment - 1);
}

static bool IsSrvDesc(const D3D12_RESOURCE_DESC& desc) {
	return !(desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE);
}

static bool IsUavDesc(const D3D12_RESOURCE_DESC& desc) {
	return desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
}

static bool IsRtvDesc(const D3D12_RESOURCE_DESC& desc) {
	return desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
}
