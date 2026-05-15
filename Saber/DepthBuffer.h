/**
 * @file DepthBuffer.h
 * @brief Manages the primary depth buffer and its associated hierarchical Z-buffer (HZB).
 */
#pragma once

#include "Headers.h"
#include "EnumFence.h"

class CommandList;
class DescRange;
class Device;
class DeviceContext;
class SinglePassDownsampler;
class TextureResource;

/**
 * @brief Lifecycle state of the depth buffer, used for fence-based synchronisation.
 */
enum class DepthBufferState : uint8_t {
    InvalidState = 0,

    DepthWriting,              /**< @brief Depth buffer is bound as a DSV and being written by the GPU. */
    HierarchicalDepthBuilding, /**< @brief SPD is computing mip levels for the HZB. */
    DepthReading,              /**< @brief HZB is bound as an SRV for GPU-side occlusion culling. */

    FlushState = std::numeric_limits<uint8_t>::max() /**< @brief Sentinel for fence-flush operations. */
};

/**
 * @brief Owns the depth texture, its hierarchical Z-buffer mip chain, and associated descriptors.
 *
 * The HZB is built by @ref SinglePassDownsampler after the depth pre-pass and
 * exposed as an SRV array for occlusion-culling compute shaders.
 */
class DepthBuffer {
    static inline D3D12_RESOURCE_DESC m_depthBufferDesc{
        CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, 0, 0, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    };
    static inline D3D12_CLEAR_VALUE m_clearValue{
        .Format{ DXGI_FORMAT_D32_FLOAT },
        .DepthStencil{ 0.0f, 0 } /**< @brief Reversed-Z clear value (0 = far plane). */
    };

    std::wstring m_name{};

    std::shared_ptr<TextureResource> m_pDepthBuffer{}; /**< @brief Full-resolution depth texture. */
    std::shared_ptr<TextureResource> m_pHZBuffer{};    /**< @brief Hierarchical Z-buffer mip chain. */

    std::shared_ptr<DescRange> m_pDsvsRange{}; /**< @brief DSV descriptor range for depth writes. */
    std::shared_ptr<DescRange> m_pSrvsRange{}; /**< @brief SRV descriptor range (depth + HZB mips). */
    size_t m_depthSrvId{};
    size_t m_hzbSrvId{};
    std::shared_ptr<DescRange> m_pUavsRange{}; /**< @brief UAV descriptor range for HZB mip writes. */

    std::shared_ptr<SinglePassDownsampler> m_pSinglePassDownsampler{};

    static const size_t m_hzbSize{ 12 };    /**< @brief Number of HZB mip levels. */
    static const size_t m_hzbMidMipId{ 5 }; /**< @brief Index of the mid-mip used by SPD's global atomic. */

    size_t m_width{};
    size_t m_height{};
    std::shared_ptr<EnumFence<DepthBufferState>> m_pDepthBufferFence{}; /**< @brief State-machine fence. */

public:
    /**
     * @brief Allocates the depth and HZB textures and their descriptors.
     * @param name           Debug name.
     * @param pDeviceContext Device context.
     * @param width          Viewport width in texels.
     * @param height         Viewport height in texels.
     * @param pSPD           Optional single-pass downsampler; pass @c nullptr to skip HZB.
     */
    DepthBuffer(
        const std::wstring& name,
        std::shared_ptr<DeviceContext> pDeviceContext,
        UINT64 width,
        UINT height,
        std::shared_ptr<SinglePassDownsampler> pSPD = nullptr
    );

    /**
     * @brief Recreates the depth texture at the new resolution; keeps descriptors in place.
     * @param pDevice Device.
     * @param width   New width in texels.
     * @param height  New height in texels.
     */
    void Resize(
        std::shared_ptr<Device> pDevice,
        UINT64 width,
        UINT height
    );

    /**
     * @brief Recreates the HZB texture at the new resolution.
     * @param pDevice Device.
     * @param width   New width in texels.
     * @param height  New height in texels.
     * @return @c true if the HZB was resized; @c false if no SPD is attached.
     */
    bool ResizeHZB(
        std::shared_ptr<Device> pDevice,
        UINT64 width,
        UINT height
    );

    /**
     * @brief Records a clear-depth command on @p pCommandList (reversed-Z: clears to 0).
     * @param pCommandList Command list.
     */
    void Clear(std::shared_ptr<CommandList> pCommandList);

    /**
     * @brief Attaches a single-pass downsampler and resizes the HZB to match.
     * @param pSPD    Downsampler instance.
     * @param pDevice Device.
     * @param width   Current viewport width.
     * @param height  Current viewport height.
     */
    void SetSinglePassDownsampler(
        std::shared_ptr<SinglePassDownsampler> pSPD,
        std::shared_ptr<Device> pDevice,
        UINT64 width,
        UINT height
    );

    /**
     * @brief Dispatches the SPD pass to build the full HZB mip chain from the depth buffer.
     * @param pCommandList Command list (compute or direct).
     * @param pDescHeap    Descriptor heap containing the SRV and UAV descriptors.
     */
    void CreateHierarchicalDepthBuffer(
        std::shared_ptr<CommandList> pCommandList,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pDescHeap
    );

    /** @brief Returns the full-resolution depth @ref TextureResource. */
    std::shared_ptr<TextureResource> GetTexture() const;

    /** @brief Returns the CPU DSV handle for binding as a depth-stencil target. */
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvCpuDescHandle() const;

    /** @brief Returns the GPU SRV handle for the full-resolution depth buffer. */
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandle() const;

    /** @brief Returns the GPU SRV handle for the depth buffer including all HZB mips. */
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuDescHandleWithMips() const;

    /** @brief Returns the GPU UAV handle for the first HZB mip (used by SPD). */
    D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandle() const;

    /** @brief Returns the GPU UAV handle for the SPD mid-mip global-atomic slot. */
    D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMidMip() const;

    /** @brief Returns the GPU UAV handle for the full HZB mip UAV array. */
    D3D12_GPU_DESCRIPTOR_HANDLE GetUavGpuDescHandleForMips() const;

    /** @brief Returns the state-machine fence used to synchronise depth-buffer phases. */
    std::shared_ptr<EnumFence<DepthBufferState>> GetFence() const;
};
