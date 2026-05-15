/**
 * @file GPUResource.h
 * @brief Base class for all D3D12 GPU resources together with view-creation helpers.
 *
 * @ref GPUResource wraps a @c D3D12MA::Allocation and the associated
 * @c ID3D12Resource, tracking the current resource state and providing
 * methods to create SRV/UAV/CBV/RTV/DSV descriptor views.
 */
#pragma once

#include "Headers.h"

#include <bit>
#include <optional>

#include "D3D12MemAlloc.h"

#include "EnumHelpers.h"

class CommandList;
class Device;
class DeviceContext;

/**
 * @brief Bitfield enum of descriptor view types that a resource may support.
 *
 * Values are powers of two so they can be combined with @ref EnumFlags.
 */
enum class ResourceView : uint8_t {
    None = 0,
    Srv  = 1 << 0, /**< @brief Shader resource view. */
    Uav  = 1 << 1, /**< @brief Unordered access view. */
    Cbv  = 1 << 2, /**< @brief Constant buffer view. */
    Rtv  = 1 << 3, /**< @brief Render target view. */
    Dsv  = 1 << 4, /**< @brief Depth-stencil view. */
    Num  = 5,
    Any  = Srv | Uav | Cbv | Rtv | Dsv /**< @brief All view types combined. */
};
ENABLE_ENUM_FLAGS(ResourceView);

/**
 * @brief Returns the concatenated name of all active view bits in @p viewType.
 * @param viewType Flag combination to stringify.
 * @return Wide string like @c L"SrvUav".
 */
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

/**
 * @brief Returns the zero-based bit-index of a single @ref ResourceView flag.
 *
 * Uses @c countr_zero so the value must have exactly one bit set.
 */
template <>
constexpr std::underlying_type_t<ResourceView> ToId<ResourceView>(ResourceView res) {
    assert(res != ResourceView::None);
    return std::countr_zero(ToUnderlying(res));
}

/**
 * @brief Converts a zero-based index back to the corresponding @ref ResourceView flag.
 * @param id Bit index (0 = Srv, 1 = Uav, …).
 * @return Corresponding @ref ResourceView value.
 */
template <>
constexpr ResourceView FromId<ResourceView>(std::underlying_type_t<ResourceView> id) {
    assert(0 <= id && id < static_cast<std::underlying_type_t<ResourceView>>(ResourceView::Num));
    return static_cast<ResourceView>(1 << id);
}

/**
 * @brief Base class for all GPU-side resources managed via D3D12MA.
 *
 * Tracks the current @c D3D12_RESOURCE_STATES, provides transition helpers,
 * and offers virtual overrides so derived classes can return typed view
 * descriptors (SRV, UAV, CBV, RTV, DSV).
 */
class GPUResource {
    static std::shared_ptr<GPUResource> pCounterResetter; /**< @brief Shared zero-counter resource used by UAV resets. */

    Microsoft::WRL::ComPtr<D3D12MA::Allocation> m_pAllocation{}; /**< @brief Memory allocation backing this resource. */

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource{}; /**< @brief Underlying D3D12 resource. */
    D3D12_RESOURCE_STATES m_state{};                      /**< @brief Current resource state. */

    GPUResource() = default;

public:
    /** @brief Heap allocation and flag parameters. */
    struct AllocationDesc {
        D3D12_HEAP_TYPE         heapType{ D3D12_HEAP_TYPE_DEFAULT };
        D3D12_HEAP_FLAGS        heapFlags{ D3D12_HEAP_FLAG_NONE };
        D3D12MA::ALLOCATION_FLAGS allocationFlags{ D3D12MA::ALLOCATION_FLAG_NONE };
    };

    /** @brief Resource description and initial state. */
    struct ResourceDesc {
        D3D12_RESOURCE_DESC resDesc{};
        D3D12_RESOURCE_STATES resInitState{ D3D12_RESOURCE_STATE_COMMON };
        const D3D12_CLEAR_VALUE* pResClearValue{};
    };

    /**
     * @brief Allocates and creates the D3D12 resource.
     * @param name      Debug name.
     * @param pDevice   Device and allocator.
     * @param allocDesc Heap type and allocation flags.
     * @param resDesc   Resource description and initial state.
     */
    GPUResource(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        const AllocationDesc& allocDesc,
        const ResourceDesc& resDesc
    );

    /** @brief Returns the current resource state. */
    D3D12_RESOURCE_STATES GetState() const {
        return m_state;
    }

    /**
     * @brief Emits a resource barrier to transition the resource to @p toState.
     * @param pCommandList Command list on which to record the barrier.
     * @param toState      Desired target state.
     */
    void ResourceTransition(
        std::shared_ptr<CommandList> pCommandList,
        const D3D12_RESOURCE_STATES& toState
    );

    /**
     * @brief (Re-)creates the D3D12 resource, replacing any existing allocation.
     * @param name      Debug name.
     * @param pDevice   Device and allocator.
     * @param allocDesc Heap parameters.
     * @param resDesc   Resource description and initial state.
     */
    void CreateResource(
        const std::wstring& name,
        std::shared_ptr<Device> pDevice,
        const AllocationDesc& allocDesc,
        const ResourceDesc& resDesc
    );

    /** @brief Returns the raw @c ID3D12Resource COM pointer. */
    Microsoft::WRL::ComPtr<ID3D12Resource> GetD3D12Resource() const;

    /** @brief Returns the @c D3D12_RESOURCE_DESC of the underlying resource. */
    D3D12_RESOURCE_DESC GetResourceDesc() const {
        return GetD3D12Resource()->GetDesc();
    }

    /** @brief Queries and returns the heap properties of the resource. */
    D3D12_HEAP_PROPERTIES GetHeapProperties() const {
        D3D12_HEAP_PROPERTIES heapProps;
        GetD3D12Resource()->GetHeapProperties(&heapProps, nullptr);
        return heapProps;
    }

    /** @brief Queries and returns the heap flags of the resource. */
    D3D12_HEAP_FLAGS GetHeapFlags() const {
        D3D12_HEAP_FLAGS heapFlags;
        GetD3D12Resource()->GetHeapProperties(nullptr, &heapFlags);
        return heapFlags;
    }

    /**
     * @brief Returns the required upload-buffer size for the given subresource range.
     * @param firstSubresource First subresource index (default 0).
     * @param numSubresources  Number of subresources (default 1).
     * @return Size in bytes needed for an intermediate upload buffer.
     */
    size_t GetIntermediateSize(
        UINT firstSubresource = 0,
        UINT numSubresources = 1
    );

    /**
     * @brief Creates an upload-heap intermediate buffer sized for the given subresource range.
     * @param pDevice          Device used to create the intermediate.
     * @param firstSubresource First subresource index.
     * @param numSubresources  Number of subresources.
     * @return Shared pointer to the intermediate @ref GPUResource.
     */
    std::shared_ptr<GPUResource> CreateIntermediate(
        std::shared_ptr<Device> pDevice,
        UINT firstSubresource = 0,
        UINT numSubresources = 1
    );

    /**
     * @brief Copies subresource data from CPU memory through an intermediate buffer to this resource.
     * @param pCommandList      Command list for copy commands.
     * @param pIntermediate     Intermediate upload buffer.
     * @param pSrcData          Array of source subresource data descriptors.
     * @param intermediateOffset Byte offset into the intermediate buffer.
     * @param firstSubresource  First destination subresource.
     * @param numSubresources   Number of subresources to copy.
     */
    void UpdateSubresources(
        std::shared_ptr<CommandList> pCommandList,
        std::shared_ptr<GPUResource>& pIntermediate,
        const D3D12_SUBRESOURCE_DATA* pSrcData,
        UINT64 intermediateOffset = 0,
        UINT firstSubresource = 0,
        UINT numSubresources = 1
    );

    /**
     * @brief Overload accepting @c D3D12_SUBRESOURCE_INFO instead of @c D3D12_SUBRESOURCE_DATA.
     */
    void UpdateSubresources(
        std::shared_ptr<CommandList> pCommandList,
        std::shared_ptr<GPUResource>& pIntermediate,
        void* pResourceData,
        const D3D12_SUBRESOURCE_INFO* pSrcData,
        UINT64 intermediateOffset = 0,
        UINT firstSubresource = 0,
        UINT numSubresources = 1
    );

    /**
     * @brief Returns @c true if the resource supports the specified view type.
     * @param viewType View to query.
     * @return @c true if a descriptor of that type can be created.
     */
    bool SupportsView(ResourceView viewType) const;

    /**
     * @brief Creates the descriptor of the given view type at @p cpuDescHandle.
     * @param viewType     Which view to create.
     * @param pDevice      Device used to write the descriptor.
     * @param cpuDescHandle Target CPU descriptor handle.
     */
    void CreateResourceView(
        ResourceView viewType,
        std::shared_ptr<Device> pDevice,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle
    );

    /** @brief Returns @c true if this resource has an SRV descriptor flag set. */
    bool IsSrv() const;
    /** @brief Returns the SRV descriptor for this resource, if supported. */
    virtual std::optional<D3D12_SHADER_RESOURCE_VIEW_DESC> GetSrvDesc() const;
    /**
     * @brief Writes a shader resource view descriptor.
     * @param pDevice       Device.
     * @param cpuDescHandle Target CPU handle.
     * @param pSrvDesc      Optional explicit SRV description; uses @ref GetSrvDesc() if null.
     */
    void CreateShaderResourceView(
        std::shared_ptr<Device> pDevice,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
        const D3D12_SHADER_RESOURCE_VIEW_DESC* pSrvDesc = nullptr
    );

    /** @brief Returns @c true if this resource has a UAV descriptor flag set. */
    bool IsUav() const;
    /** @brief Returns the UAV descriptor for this resource, if supported. */
    virtual std::optional<D3D12_UNORDERED_ACCESS_VIEW_DESC> GetUavDesc() const;
    /**
     * @brief Writes an unordered access view descriptor.
     * @param pDevice          Device.
     * @param cpuDescHandle    Target CPU handle.
     * @param pUavDesc         Optional explicit UAV description.
     * @param pCounterResource Optional counter resource for structured-buffer counters.
     */
    void CreateUnorderedAccessView(
        std::shared_ptr<Device> pDevice,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
        const D3D12_UNORDERED_ACCESS_VIEW_DESC* pUavDesc = nullptr,
        Microsoft::WRL::ComPtr<ID3D12Resource> pCounterResource = nullptr
    );

    /** @brief Returns @c true if this resource has a CBV descriptor flag set. */
    bool IsCbv() const;
    /** @brief Returns the CBV descriptor for this resource, if supported. */
    virtual std::optional<D3D12_CONSTANT_BUFFER_VIEW_DESC> GetCbvDesc() const;
    /**
     * @brief Writes a constant buffer view descriptor.
     * @param pDevice       Device.
     * @param cpuDescHandle Target CPU handle.
     * @param pCbvDesc      Optional explicit CBV description.
     */
    void CreateConstantBufferView(
        std::shared_ptr<Device> pDevice,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
        const D3D12_CONSTANT_BUFFER_VIEW_DESC* pCbvDesc = nullptr
    );

    /** @brief Returns @c true if this resource has an RTV descriptor flag set. */
    bool IsRtv() const;
    /** @brief Returns the RTV descriptor for this resource, if supported. */
    virtual std::optional<D3D12_RENDER_TARGET_VIEW_DESC> GetRtvDesc() const;
    /**
     * @brief Writes a render target view descriptor.
     * @param pDevice       Device.
     * @param cpuDescHandle Target CPU handle.
     * @param pRtvDesc      Optional explicit RTV description.
     */
    void CreateRenderTargetView(
        std::shared_ptr<Device> pDevice,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
        const D3D12_RENDER_TARGET_VIEW_DESC* pRtvDesc = nullptr
    );

    /** @brief Returns @c true if this resource has a DSV descriptor flag set. */
    bool IsDsv() const;
    /** @brief Returns the DSV descriptor for this resource, if supported. */
    virtual std::optional<D3D12_DEPTH_STENCIL_VIEW_DESC> GetDsvDesc() const;
    /**
     * @brief Writes a depth-stencil view descriptor.
     * @param pDevice       Device.
     * @param cpuDescHandle Target CPU handle.
     * @param pDsvDesc      Optional explicit DSV description.
     */
    void CreateDepthStencilView(
        std::shared_ptr<Device> pDevice,
        const D3D12_CPU_DESCRIPTOR_HANDLE& cpuDescHandle,
        const D3D12_DEPTH_STENCIL_VIEW_DESC* pDsvDesc = nullptr
    );

    /**
     * @brief Records a clear-render-target command for this resource.
     * @param pCommandList  Command list.
     * @param cpuDescHandle RTV CPU handle to clear.
     * @param clearColor    RGBA clear colour; uses the resource's optimised clear if null.
     */
    void ClearRenderTarget(
        std::shared_ptr<CommandList> pCommandList,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
        const float* clearColor = nullptr
    );

    /**
     * @brief Records a clear-depth-stencil command for this resource.
     * @param pCommandList  Command list.
     * @param cpuDescHandle DSV CPU handle to clear.
     * @param depth         Depth clear value (default 0.0 for reversed-Z).
     * @param clearFlags    Which aspects to clear (default depth only).
     * @param stencil       Stencil clear value.
     */
    void ClearDepthTarget(
        std::shared_ptr<CommandList> pCommandList,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle,
        float depth = 0.f,
        const D3D12_CLEAR_FLAGS& clearFlags = D3D12_CLEAR_FLAG_DEPTH,
        uint8_t stencil = 0
    );

    /**
     * @brief Initialises the global zero-counter resource used by @ref ResetCounter.
     * @param pDevice      Device context providing the D3D12 device.
     * @param pCommandList Command list used to upload the zero value.
     */
    static void InitCounterResetter(
        std::shared_ptr<DeviceContext> pDevice,
        std::shared_ptr<CommandList> pCommandList
    );

    /** @brief Releases the global counter-reset resource. */
    static void DestroyCounterResetter();

    /**
     * @brief Copies a zero value to the UAV counter at @p counterOffset bytes into this resource.
     * @param pCommandList  Command list.
     * @param counterOffset Byte offset of the counter within the resource.
     */
    void ResetCounter(
        std::shared_ptr<CommandList> pCommandList,
        uint64_t counterOffset
    ) const;
};

// ----------------------------------------------------------------- helpers

/**
 * @brief Rounds @p size up to the nearest multiple of @p alignment.
 * @param size      Value to align.
 * @param alignment Alignment boundary (must be a power of two).
 * @return Aligned size.
 */
UINT AlignSize(UINT size, UINT alignment);

/** @brief Returns @c true if the resource description allows CBV creation. */
bool SupportsView(ResourceView viewType, const D3D12_RESOURCE_DESC& desc);
bool IsCbvDesc(const D3D12_RESOURCE_DESC& desc);
bool IsSrvDesc(const D3D12_RESOURCE_DESC& desc);
bool IsUavDesc(const D3D12_RESOURCE_DESC& desc);
bool IsRtvDesc(const D3D12_RESOURCE_DESC& desc);
bool IsDsvDesc(const D3D12_RESOURCE_DESC& desc);
