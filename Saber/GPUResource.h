#pragma once

#include "Headers.h"

#include <bit>
#include <optional>

#include "D3D12MemAlloc.h"

#include "EnumHelpers.h"

class CommandList;
class Device;
class DeviceContext;

// ResourceView and helper functions
enum class ResourceView : uint8_t {
	None = 0,
	Srv = 1 << 0,
	Uav = 1 << 1,
	Cbv = 1 << 2,
	Rtv = 1 << 3,
	Dsv = 1 << 4,
	Num = 5,
	Any = Srv | Uav | Cbv | Rtv | Dsv
};
ENABLE_ENUM_FLAGS(ResourceView);

constexpr std::wstring ToName(EnumFlags<ResourceView> viewType) {
	if (viewType == ResourceView::None) return L"None";

	std::wstring result;
	if (viewType & ResourceView::Srv) result += L"Srv";
	if (viewType & ResourceView::Uav) result += L"Uav";
	if (viewType & ResourceView::Cbv) result += L"Cbv";
	if (viewType & ResourceView::Rtv) result += L"Rtv";
	if (viewType & ResourceView::Dsv) result += L"Dsv";

	return result.empty() ? L"UnknownResourceViewType" : result;
}

template <>
constexpr std::underlying_type_t<ResourceView> ToId<ResourceView>(ResourceView res) {
	assert(res != ResourceView::None);
	return std::countr_zero(ToUnderlying(res));
}
template <>
constexpr ResourceView FromId<ResourceView>(std::underlying_type_t<ResourceView> id) {
	assert(0 <= id && id < static_cast<std::underlying_type_t<ResourceView>>(ResourceView::Num));
	return static_cast<ResourceView>(1 << id);
}

// GPUResource class
class GPUResource {
	static std::shared_ptr<GPUResource> pCounterResetter;

	Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{};

protected:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource{};
	D3D12_RESOURCE_STATES m_state{};

	GPUResource() = default;

public:
	struct AllocationDesc {
		D3D12_HEAP_TYPE heapType{ D3D12_HEAP_TYPE_DEFAULT };
		D3D12_HEAP_FLAGS heapFlags{ D3D12_HEAP_FLAG_NONE };
		D3D12MA::ALLOCATION_FLAGS allocationFlags{ D3D12MA::ALLOCATION_FLAG_NONE };
	};
	struct ResourceDesc {
		D3D12_RESOURCE_DESC resDesc{};
		D3D12_RESOURCE_STATES resInitState{ D3D12_RESOURCE_STATE_COMMON };
		const D3D12_CLEAR_VALUE* pResClearValue{};
	};
	GPUResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const AllocationDesc& allocDesc,
		const ResourceDesc& resDesc
	);

	D3D12_RESOURCE_STATES GetState() const {
		return m_state;
	}
	void ResourceTransition(
		std::shared_ptr<CommandList> pCommandList,
		const D3D12_RESOURCE_STATES& toState
	);

	void CreateResource(
		const std::wstring& name,
		std::shared_ptr<Device> pDevice,
		const AllocationDesc& allocDesc,
		const ResourceDesc& resDesc
	);

	Microsoft::WRL::ComPtr<ID3D12Resource> GetD3D12Resource() const;

	D3D12_RESOURCE_DESC GetResourceDesc() const {
		return GetD3D12Resource()->GetDesc();
	}

	D3D12_HEAP_PROPERTIES GetHeapProperties() const {
		D3D12_HEAP_PROPERTIES heapProps;
		GetD3D12Resource()->GetHeapProperties(&heapProps, nullptr);
		return heapProps;
	}

	D3D12_HEAP_FLAGS GetHeapFlags() const {
		D3D12_HEAP_FLAGS heapFlags;
		GetD3D12Resource()->GetHeapProperties(nullptr, &heapFlags);
		return heapFlags;
	}

	size_t GetIntermediateSize(
		UINT firstSubresource = 0,
		UINT numSubresources = 1
	);

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

	bool SupportsView(ResourceView viewType) const;
	void CreateResourceView(
		ResourceView viewType,
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle
	);

	bool IsSrv() const;
	virtual std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> GetSrvDesc() const;
	void CreateShaderResourceView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr
	);

	bool IsUav() const;
	virtual std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> GetUavDesc() const;
	void CreateUnorderedAccessView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc = nullptr,
		Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource = nullptr
	);

	bool IsCbv() const;
	virtual std::optional<D3D12_CONSTANT_BUFFER_VIEW_DESC> GetCbvDesc() const;
	void CreateConstantBufferView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc = nullptr
	);

	bool IsRtv() const;
	virtual std::optional<D3D12_RENDER_TARGET_VIEW_DESC> GetRtvDesc() const;
	void CreateRenderTargetView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc = nullptr
	);

	bool IsDsv() const;
	virtual std::optional<D3D12_DEPTH_STENCIL_VIEW_DESC> GetDsvDesc() const;
	void CreateDepthStencilView(
		std::shared_ptr<Device> pDevice,
		const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
		const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc = nullptr
	);

	void ClearRenderTarget(
		std::shared_ptr<CommandList> pCommandList,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
		const float* clearColor = nullptr
	);

	void ClearDepthTarget(
		std::shared_ptr<CommandList> pCommandList,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
		float depth = 0.f,
		const D3D12_CLEAR_FLAGS& clearFlags = D3D12_CLEAR_FLAG_DEPTH,
		uint8_t stencil = 0
	);

	// CounterResetter related methods
	static void InitCounterResetter(
		std::shared_ptr<DeviceContext> pDevice,
		std::shared_ptr<CommandList> pCommandList
	);
	static void DestroyCounterResetter();
	void ResetCounter(
		std::shared_ptr<CommandList> pCommandList,
		uint64_t counterOffset
	) const;
};

// Helper functions
UINT AlignSize(UINT size, UINT alignment);

bool SupportsView(ResourceView viewType, const D3D12_RESOURCE_DESC& desc);
bool IsCbvDesc(const D3D12_RESOURCE_DESC& desc);
bool IsSrvDesc(const D3D12_RESOURCE_DESC& desc);
bool IsUavDesc(const D3D12_RESOURCE_DESC& desc);
bool IsRtvDesc(const D3D12_RESOURCE_DESC& desc);
bool IsDsvDesc(const D3D12_RESOURCE_DESC& desc);
